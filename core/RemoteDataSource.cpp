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
            emit dataChanged(msg.value(QStringLiteral("table")).toString());
        }
    }
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

    m_pending.remove(reqId);
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
