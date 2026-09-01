#include "Server.h"

#include "common/Protocol.h"

#include <QDebug>
#include <QHostAddress>
#include <QJsonArray>

Server::Server(QObject *parent)
    : QObject(parent)
{
}

bool Server::start(const QString &dbPath, const QString &listenAddress, quint16 port)
{
    m_ds = new LocalDataSource(this);
    if (!m_ds->open(dbPath))
        return false;

    seedInitialData(); // 管理员账号等基础数据(幂等,已有则跳过)

    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &Server::onNewConnection);
    connect(m_ds, &DataSource::dataChanged, this, &Server::broadcastDataChanged);

    if (!m_server->listen(QHostAddress(listenAddress), port)) {
        qWarning() << "[Server] 监听失败:" << m_server->errorString();
        return false;
    }
    qInfo() << "[Server] 已监听" << listenAddress << "端口" << m_server->serverPort();
    return true;
}

bool Server::isRunning() const
{
    return m_server && m_server->isListening();
}

quint16 Server::port() const
{
    return m_server ? m_server->serverPort() : 0;
}

void Server::seedInitialData()
{
    // 幂等:admin 账号已存在则不动
    const QueryResult admins = m_ds->query(QStringLiteral("admins"));
    if (!admins.isEmpty())
        return;

    QHash<QString, QVariant> admin;
    admin.insert(QStringLiteral("username"), QStringLiteral("admin"));
    admin.insert(QStringLiteral("password"), QStringLiteral("123456"));
    const qlonglong id = m_ds->insertRow(QStringLiteral("admins"), admin);
    qInfo() << "[Server] 已初始化默认管理员 admin, id =" << id;
}

void Server::onNewConnection()
{
    while (QTcpSocket *client = m_server->nextPendingConnection()) {
        m_clients.insert(client);
        m_buffers.insert(client, QByteArray());
        connect(client, &QTcpSocket::readyRead, this, &Server::onReadyRead);
        connect(client, &QTcpSocket::disconnected, this, &Server::onClientDisconnected);
        qInfo() << "[Server] 客户端接入:" << client->peerAddress().toString()
                << ":" << client->peerPort();
    }
}

void Server::onReadyRead()
{
    QTcpSocket *client = qobject_cast<QTcpSocket *>(sender());
    if (!client)
        return;

    QByteArray &buffer = m_buffers[client];
    buffer.append(client->readAll());

    QJsonObject msg;
    while (Protocol::tryDecodeFrame(buffer, msg))
        handleMessage(client, msg);
}

void Server::onClientDisconnected()
{
    QTcpSocket *client = qobject_cast<QTcpSocket *>(sender());
    if (!client)
        return;

    qInfo() << "[Server] 客户端断开:" << client->peerAddress().toString()
            << ":" << client->peerPort();
    m_buffers.remove(client);
    m_clients.remove(client);
    client->deleteLater();
}

void Server::sendToClient(QTcpSocket *client, const QJsonObject &msg)
{
    client->write(Protocol::encodeFrame(msg));
}

void Server::broadcastDataChanged(const QString &table)
{
    const QByteArray frame = Protocol::encodeFrame(Protocol::makeDataChanged(table));
    for (QTcpSocket *client : m_clients) {
        if (client->state() == QAbstractSocket::ConnectedState)
            client->write(frame);
    }
    qInfo() << "[Server] 广播 dataChanged:" << table;
}

void Server::handleMessage(QTcpSocket *client, const QJsonObject &msg)
{
    const quint32 reqId = static_cast<quint32>(msg.value(QStringLiteral("reqId")).toInt());
    const QString op = msg.value(QStringLiteral("op")).toString();
    const QString table = msg.value(QStringLiteral("table")).toString();

    QJsonObject resp;
    resp.insert(QStringLiteral("kind"), QStringLiteral("resp"));
    resp.insert(QStringLiteral("reqId"), QJsonValue(static_cast<qint64>(reqId)));

    if (op == QStringLiteral("query")) {
        QStringList fields;
        const QJsonArray fArr = msg.value(QStringLiteral("fields")).toArray();
        for (const QJsonValue &v : fArr)
            fields << v.toString();

        QVariantList bind;
        const QJsonArray bArr = msg.value(QStringLiteral("bind")).toArray();
        for (const QJsonValue &v : bArr)
            bind << Protocol::jsonToVariant(v);

        const QueryResult rows = m_ds->query(table, fields,
                                             msg.value(QStringLiteral("where")).toString(), bind);
        QJsonArray arr;
        for (const DataRow &row : rows) {
            QJsonObject obj;
            for (auto it = row.cbegin(); it != row.cend(); ++it)
                obj.insert(it.key(), Protocol::variantToJson(it.value()));
            arr.append(obj);
        }
        resp.insert(QStringLiteral("ok"), QJsonValue(true));
        resp.insert(QStringLiteral("data"), QJsonValue(arr));
    } else if (op == QStringLiteral("insert")) {
        const QHash<QString, QVariant> values =
            Protocol::jsonToValues(msg.value(QStringLiteral("values")).toObject());
        const qlonglong id = m_ds->insertRow(table, values);
        if (id >= 0) {
            resp.insert(QStringLiteral("ok"), QJsonValue(true));
            resp.insert(QStringLiteral("data"), QJsonValue(static_cast<qint64>(id)));
        } else {
            resp.insert(QStringLiteral("ok"), QJsonValue(false));
            resp.insert(QStringLiteral("error"), QStringLiteral("insert failed"));
        }
    } else if (op == QStringLiteral("update")) {
        const QHash<QString, QVariant> values =
            Protocol::jsonToValues(msg.value(QStringLiteral("values")).toObject());
        QVariantList bind;
        const QJsonArray bArr = msg.value(QStringLiteral("bind")).toArray();
        for (const QJsonValue &v : bArr)
            bind << Protocol::jsonToVariant(v);
        const int affected = m_ds->updateRows(table, values,
                                              msg.value(QStringLiteral("where")).toString(), bind);
        resp.insert(QStringLiteral("ok"), QJsonValue(true));
        resp.insert(QStringLiteral("data"), QJsonValue(affected));
    } else if (op == QStringLiteral("remove")) {
        QVariantList bind;
        const QJsonArray bArr = msg.value(QStringLiteral("bind")).toArray();
        for (const QJsonValue &v : bArr)
            bind << Protocol::jsonToVariant(v);
        const int affected = m_ds->removeRows(table,
                                              msg.value(QStringLiteral("where")).toString(), bind);
        resp.insert(QStringLiteral("ok"), QJsonValue(true));
        resp.insert(QStringLiteral("data"), QJsonValue(affected));
    } else {
        resp.insert(QStringLiteral("ok"), QJsonValue(false));
        resp.insert(QStringLiteral("error"), QStringLiteral("unknown op: ") + op);
    }

    sendToClient(client, resp);
}
