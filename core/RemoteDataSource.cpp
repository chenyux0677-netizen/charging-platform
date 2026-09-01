#include "RemoteDataSource.h"

#include "common/Protocol.h"

#include <QDebug>
#include <QEventLoop>
#include <QTimer>

RemoteDataSource::RemoteDataSource(QObject *parent)
    : DataSource(parent)
{
}

bool RemoteDataSource::connectToServer(const QString &host, quint16 port, int timeoutMs)
{
    if (m_socket) {
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_buffer.clear();

    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &RemoteDataSource::connected);
    connect(m_socket, &QTcpSocket::disconnected, this, &RemoteDataSource::disconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this,
            [this] { emit connectionError(m_socket ? m_socket->errorString() : QString()); });
    connect(m_socket, &QTcpSocket::readyRead, this, &RemoteDataSource::onReadyRead);

    m_socket->connectToHost(host, port);
    if (!m_socket->waitForConnected(timeoutMs)) {
        qWarning() << "[RemoteDataSource] 连接失败:" << m_socket->errorString();
        m_connected = false;
        return false;
    }
    m_connected = true;
    emit connected();
    return true;
}

bool RemoteDataSource::isConnected() const
{
    return m_connected && m_socket
        && m_socket->state() == QAbstractSocket::ConnectedState;
}

void RemoteDataSource::onReadyRead()
{
    m_buffer.append(m_socket->readAll());

    QJsonObject msg;
    while (Protocol::tryDecodeFrame(m_buffer, msg)) {
        const QString kind = msg.value(QStringLiteral("kind")).toString();

        if (kind == QStringLiteral("resp")) {
            const quint32 reqId = static_cast<quint32>(msg.value(QStringLiteral("reqId")).toInt());
            const auto it = m_pending.find(reqId);
            if (it == m_pending.end())
                continue; // 不认识的 reqId,丢弃
            const auto callback = it.value();
            m_pending.erase(it);

            const bool ok = msg.value(QStringLiteral("ok")).toBool();
            if (ok) {
                callback(Protocol::jsonToVariant(msg.value(QStringLiteral("data"))));
            } else {
                qWarning() << "[RemoteDataSource] 服务器返回错误:"
                           << msg.value(QStringLiteral("error")).toString();
                callback(QVariant());
            }
        } else if (kind == QStringLiteral("notify")) {
            // 暂存:请求进行中(含嵌套)同步刷新会重入,统一延迟到请求结束再补发
            m_deferredNotifies << msg.value(QStringLiteral("table")).toString();
        }
    }
    // 通知统一延迟发出:绝不在此处同步刷新(详见 flushDeferredNotifies)
    scheduleFlush();
}

void RemoteDataSource::flushDeferredNotifies()
{
    m_flushScheduled = false;
    // 还有请求在等回包时先不发,等该请求结束再重新排队
    if (!m_pending.isEmpty() || m_deferredNotifies.isEmpty())
        return;
    const QStringList tables = m_deferredNotifies;
    m_deferredNotifies.clear();
    for (const QString &t : tables)
        emit dataChanged(t);
}

void RemoteDataSource::scheduleFlush()
{
    if (m_flushScheduled)
        return;
    if (!m_pending.isEmpty() || m_deferredNotifies.isEmpty())
        return;
    m_flushScheduled = true;
    // 0ms 定时器:等当前事件(可能含 onReadyRead)处理完,在普通上下文中发通知。
    // 页面收到后做的阻塞刷新查询因此能正常收到回包,不会触发 readyRead 屏蔽超时。
    QTimer::singleShot(0, this, [this] { flushDeferredNotifies(); });
}

QVariant RemoteDataSource::sendRequestAndWait(const QJsonObject &request)
{
    if (!m_connected || !m_socket)
        return QVariant();

    const quint32 reqId = static_cast<quint32>(request.value(QStringLiteral("reqId")).toInt());

    QVariant result;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    bool timedOut = false;
    QObject::connect(&timer, &QTimer::timeout, [&] { timedOut = true; loop.quit(); });
    m_pending.insert(reqId, [&](const QVariant &data) { result = data; loop.quit(); });

    m_socket->write(Protocol::encodeFrame(request));
    timer.start(5000); // 5 秒超时,避免永久阻塞
    loop.exec();
    timer.stop(); // 回包后立即停表,防止过期定时器串扰后续嵌套请求

    m_pending.remove(reqId);
    scheduleFlush();
    if (timedOut) {
        qWarning() << "[RemoteDataSource] 请求超时(reqId =" << reqId << ")";
        return QVariant();
    }
    return result;
}

QueryResult RemoteDataSource::query(const QString &table,
                                    const QStringList &fields,
                                    const QString &where,
                                    const QVariantList &bindValues)
{
    const quint32 reqId = ++m_nextReqId;
    const QJsonObject request = Protocol::makeRequest(reqId, QStringLiteral("query"), table,
                                                      fields, where, bindValues);
    const QVariant data = sendRequestAndWait(request);

    QueryResult result;
    const QVariantList rows = data.toList();
    for (const QVariant &rv : rows) {
        const QVariantMap rowMap = rv.toMap();
        DataRow row;
        for (auto it = rowMap.cbegin(); it != rowMap.cend(); ++it)
            row.insert(it.key(), it.value());
        result << row;
    }
    return result;
}

qlonglong RemoteDataSource::insertRow(const QString &table,
                                      const QHash<QString, QVariant> &values)
{
    const quint32 reqId = ++m_nextReqId;
    const QJsonObject request = Protocol::makeRequest(reqId, QStringLiteral("insert"), table,
                                                      {}, QString(), {}, values);
    const QVariant data = sendRequestAndWait(request);
    return data.isValid() ? data.toLongLong() : -1;
}

int RemoteDataSource::updateRows(const QString &table,
                                 const QHash<QString, QVariant> &values,
                                 const QString &where,
                                 const QVariantList &bindValues)
{
    const quint32 reqId = ++m_nextReqId;
    const QJsonObject request = Protocol::makeRequest(reqId, QStringLiteral("update"), table,
                                                      {}, where, bindValues, values);
    const QVariant data = sendRequestAndWait(request);
    return data.isValid() ? data.toInt() : 0;
}

int RemoteDataSource::removeRows(const QString &table,
                                 const QString &where,
                                 const QVariantList &bindValues)
{
    const quint32 reqId = ++m_nextReqId;
    const QJsonObject request = Protocol::makeRequest(reqId, QStringLiteral("remove"), table,
                                                      {}, where, bindValues);
    const QVariant data = sendRequestAndWait(request);
    return data.isValid() ? data.toInt() : 0;
}
