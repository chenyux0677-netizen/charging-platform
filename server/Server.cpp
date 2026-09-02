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
    if (admins.isEmpty()) {
        QHash<QString, QVariant> admin;
        admin.insert(QStringLiteral("username"), QStringLiteral("admin"));
        admin.insert(QStringLiteral("password"), QStringLiteral("123456"));
        const qlonglong id = m_ds->insertRow(QStringLiteral("admins"), admin);
        qInfo() << "[Server] 已初始化默认管理员 admin, id =" << id;
    }

    // 演示种子数据:电站 + 电桩。仅当电站表为空时写入。
    // ★ 这是"演示数据",不是真实设备:仅供首次运行开箱即用。
    //   之后可在管理员端"充电站管理"自行新增/修改,或删除 app.db 重置。
    if (!m_ds->query(QStringLiteral("charging_stations")).isEmpty())
        return;

    struct PileGroup { int count; QString type; double power; };
    struct SeedStation { QString name, address; double lat, lng, price; QVector<PileGroup> piles; };

    const QVector<SeedStation> seeds = {
        { QStringLiteral("中关村科技园充电站"), QStringLiteral("北京市海淀区中关村大街1号"),
          39.984, 116.318, 1.20,
          { {4, QStringLiteral("快充"), 120.0}, {2, QStringLiteral("慢充"), 7.0} } },
        { QStringLiteral("陆家嘴金融城充电站"), QStringLiteral("上海市浦东新区世纪大道100号"),
          31.239, 121.501, 1.50,
          { {3, QStringLiteral("快充"), 120.0}, {2, QStringLiteral("慢充"), 7.0} } },
        { QStringLiteral("天河CBD充电站"), QStringLiteral("广州市天河区体育西路108号"),
          23.129, 113.321, 1.35,
          { {3, QStringLiteral("快充"), 60.0}, {2, QStringLiteral("慢充"), 7.0} } },
    };

    for (const SeedStation &s : seeds) {
        QHash<QString, QVariant> st;
        st.insert(QStringLiteral("name"), s.name);
        st.insert(QStringLiteral("address"), s.address);
        st.insert(QStringLiteral("lat"), s.lat);
        st.insert(QStringLiteral("lng"), s.lng);
        st.insert(QStringLiteral("price_per_kwh"), s.price);
        const qlonglong sid = m_ds->insertRow(QStringLiteral("charging_stations"), st);
        if (sid <= 0)
            continue;

        int idx = 0;
        for (const PileGroup &g : s.piles) {
            for (int i = 0; i < g.count; ++i) {
                ++idx;
                QHash<QString, QVariant> p;
                p.insert(QStringLiteral("station_id"), sid);
                p.insert(QStringLiteral("code"),
                         QStringLiteral("P%1-%2").arg(sid).arg(idx, 2, 10, QChar('0')));
                p.insert(QStringLiteral("type"), g.type);
                p.insert(QStringLiteral("power_kw"), g.power);
                p.insert(QStringLiteral("price_per_kwh"), s.price);
                m_ds->insertRow(QStringLiteral("charging_piles"), p);
            }
        }
    }
    qInfo() << "[Server] 已写入演示电站/电桩种子数据(删除 app.db 可重置)";
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
    qInfo() << "[Server] sendToClient reqId" << msg.value("reqId").toInt();
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
    qInfo() << "[Server] handle op=" << op << "table=" << table << "reqId=" << reqId;

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
    } else if (op == QStringLiteral("startCharge")) {
        const QHash<QString, QVariant> values =
            Protocol::jsonToValues(msg.value(QStringLiteral("values")).toObject());
        const qlonglong orderId = m_ds->startCharge(
            values.value(QStringLiteral("userId")).toLongLong(),
            values.value(QStringLiteral("pileId")).toLongLong());
        if (orderId > 0) {
            resp.insert(QStringLiteral("ok"), QJsonValue(true));
            resp.insert(QStringLiteral("data"), QJsonValue(static_cast<qint64>(orderId)));
        } else {
            resp.insert(QStringLiteral("ok"), QJsonValue(false));
            resp.insert(QStringLiteral("error"),
                        QStringLiteral("开始充电失败:充电桩不可用或账号已有进行中订单"));
        }
    } else if (op == QStringLiteral("settleCharge")) {
        const QHash<QString, QVariant> values =
            Protocol::jsonToValues(msg.value(QStringLiteral("values")).toObject());
        const bool ok = m_ds->settleCharge(
            values.value(QStringLiteral("orderId")).toLongLong());
        if (ok) {
            resp.insert(QStringLiteral("ok"), QJsonValue(true));
            resp.insert(QStringLiteral("data"), QJsonValue(true));
        } else {
            resp.insert(QStringLiteral("ok"), QJsonValue(false));
            resp.insert(QStringLiteral("error"),
                        QStringLiteral("结算失败:订单不存在/已结算或余额不足"));
        }
    } else if (op == QStringLiteral("rechargeBalance")) {
        const QHash<QString, QVariant> values =
            Protocol::jsonToValues(msg.value(QStringLiteral("values")).toObject());
        const bool ok = m_ds->rechargeBalance(
            values.value(QStringLiteral("userId")).toLongLong(),
            values.value(QStringLiteral("amount")).toDouble());
        if (ok) {
            resp.insert(QStringLiteral("ok"), QJsonValue(true));
            resp.insert(QStringLiteral("data"), QJsonValue(true));
        } else {
            resp.insert(QStringLiteral("ok"), QJsonValue(false));
            resp.insert(QStringLiteral("error"),
                        QStringLiteral("充值失败:金额不合法或用户不存在/已冻结"));
        }
    } else if (op == QStringLiteral("removeChargingPile")) {
        const QHash<QString, QVariant> values =
            Protocol::jsonToValues(msg.value(QStringLiteral("values")).toObject());
        const bool ok = m_ds->removeChargingPile(
            values.value(QStringLiteral("pileId")).toLongLong());
        if (ok) {
            resp.insert(QStringLiteral("ok"), QJsonValue(true));
            resp.insert(QStringLiteral("data"), QJsonValue(true));
        } else {
            resp.insert(QStringLiteral("ok"), QJsonValue(false));
            resp.insert(QStringLiteral("error"),
                        QStringLiteral("该充电桩正在使用或已有订单记录,不能删除"));
        }
    } else if (op == QStringLiteral("removeChargingStation")) {
        const QHash<QString, QVariant> values =
            Protocol::jsonToValues(msg.value(QStringLiteral("values")).toObject());
        const bool ok = m_ds->removeChargingStation(
            values.value(QStringLiteral("stationId")).toLongLong());
        if (ok) {
            resp.insert(QStringLiteral("ok"), QJsonValue(true));
            resp.insert(QStringLiteral("data"), QJsonValue(true));
        } else {
            resp.insert(QStringLiteral("ok"), QJsonValue(false));
            resp.insert(QStringLiteral("error"),
                        QStringLiteral("该电站包含使用中或已有订单记录的充电桩,不能删除"));
        }
    } else {
        resp.insert(QStringLiteral("ok"), QJsonValue(false));
        resp.insert(QStringLiteral("error"), QStringLiteral("unknown op: ") + op);
    }

    sendToClient(client, resp);
}
