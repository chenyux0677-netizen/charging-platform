#include "Server.h"

#include "common/Protocol.h"
#include "core/PasswordHasher.h"

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
        const QString salt = PasswordHasher::generateSalt();
        admin.insert(QStringLiteral("password_salt"), salt);
        admin.insert(QStringLiteral("password_hash"),
                     PasswordHasher::derive(QStringLiteral("123456"), salt));
        const qlonglong id = m_ds->insertRow(QStringLiteral("admins"), admin);
        qInfo() << "[Server] 已初始化默认管理员 admin, id =" << id;
    }

    // 演示种子数据:电站 + 电桩。仅当电站表为空时写入。
    // ★ 这是"演示数据",不是真实设备:仅供首次运行开箱即用。
    //   之后可在管理员端"充电站管理"自行新增/修改,或删除 app.db 重置。
    if (!m_ds->query(QStringLiteral("charging_stations")).isEmpty())
        return;

    struct PileGroup { int count; QString type; double power, price; };
    struct SeedStation { QString name, address; double lat, lng; QVector<PileGroup> piles; };

    const QVector<SeedStation> seeds = {
        { QStringLiteral("北理1号充电站"), QStringLiteral("北京市海淀区中关村南大街5号"),
          39.959951, 116.315227,
          { {4, QStringLiteral("快充"), 120.0, 1.20},
            {2, QStringLiteral("慢充"),   7.0, 1.00} } },
        { QStringLiteral("陆家嘴金融城充电站"), QStringLiteral("上海市浦东新区世纪大道100号"),
          31.239, 121.501,
          { {3, QStringLiteral("快充"), 120.0, 1.50},
            {2, QStringLiteral("慢充"),   7.0, 1.20} } },
        { QStringLiteral("天河CBD充电站"), QStringLiteral("广州市天河区体育西路108号"),
          23.129, 113.321,
          { {3, QStringLiteral("快充"), 60.0, 1.35},
            {2, QStringLiteral("慢充"),  7.0, 1.00} } },
    };

    for (const SeedStation &s : seeds) {
        QHash<QString, QVariant> st;
        st.insert(QStringLiteral("name"), s.name);
        st.insert(QStringLiteral("address"), s.address);
        st.insert(QStringLiteral("lat"), s.lat);
        st.insert(QStringLiteral("lng"), s.lng);
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
                         QStringLiteral("%1").arg(idx, 2, 10, QChar('0')));
                p.insert(QStringLiteral("type"), g.type);
                p.insert(QStringLiteral("power_kw"), g.power);
                p.insert(QStringLiteral("price_per_kwh"), g.price);
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
        m_sessions.insert(client, ClientSession());
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
    m_sessions.remove(client);
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
        if (client->state() == QAbstractSocket::ConnectedState
            && m_sessions.value(client).role != ClientSession::None)
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

    const auto values = [&msg] {
        return Protocol::jsonToValues(msg.value(QStringLiteral("values")).toObject());
    };
    const auto bindValues = [&msg] {
        QVariantList bind;
        const QJsonArray array = msg.value(QStringLiteral("bind")).toArray();
        for (const QJsonValue &value : array)
            bind << Protocol::jsonToVariant(value);
        return bind;
    };
    const auto setError = [&resp](const QString &error) {
        resp.insert(QStringLiteral("ok"), false);
        resp.insert(QStringLiteral("error"), error);
    };
    const auto setRows = [&resp](const QueryResult &rows) {
        QJsonArray array;
        for (const DataRow &row : rows) {
            QJsonObject object;
            for (auto it = row.cbegin(); it != row.cend(); ++it)
                object.insert(it.key(), Protocol::variantToJson(it.value()));
            array.append(object);
        }
        resp.insert(QStringLiteral("ok"), true);
        resp.insert(QStringLiteral("data"), array);
    };

    ClientSession &session = m_sessions[client];
    if (op == QStringLiteral("loginAdmin")) {
        const auto credentials = values();
        const QString username = credentials.value(QStringLiteral("username")).toString();
        const bool ok = m_ds->loginAdmin(
            username, credentials.value(QStringLiteral("password")).toString());
        if (ok) {
            session.role = ClientSession::Admin;
            session.userId = 0;
            session.adminUsername = username;
            resp.insert(QStringLiteral("ok"), true);
            resp.insert(QStringLiteral("data"), true);
        } else {
            setError(QStringLiteral("账号或密码错误"));
        }
        sendToClient(client, resp);
        return;
    }
    if (op == QStringLiteral("loginUser")) {
        const DataRow user = m_ds->loginUser(
            values().value(QStringLiteral("phone")).toString());
        if (user.isEmpty()) {
            setError(QStringLiteral("手机号无效、账号被冻结或登录失败"));
        } else {
            session.role = ClientSession::User;
            session.userId = user.value(QStringLiteral("id")).toLongLong();
            session.adminUsername.clear();
            QJsonObject object;
            for (auto it = user.cbegin(); it != user.cend(); ++it)
                object.insert(it.key(), Protocol::variantToJson(it.value()));
            resp.insert(QStringLiteral("ok"), true);
            resp.insert(QStringLiteral("data"), object);
        }
        sendToClient(client, resp);
        return;
    }

    if (session.role == ClientSession::None) {
        setError(QStringLiteral("请先登录"));
        sendToClient(client, resp);
        return;
    }

    // 用户连接只开放自身业务所需的固定请求形状；管理员连接才可使用通用 CRUD。
    if (session.role == ClientSession::User) {
        if (op == QStringLiteral("query")) {
            const QString where = msg.value(QStringLiteral("where")).toString();
            const QVariantList bind = bindValues();
            QueryResult rows;
            bool allowed = false;
            if (table == QStringLiteral("charging_stations")) {
                if (where.isEmpty() && bind.isEmpty()) {
                    rows = m_ds->query(table);
                    allowed = true;
                } else if (where == QStringLiteral("id = ?") && bind.size() == 1) {
                    rows = m_ds->query(table, {}, where, bind);
                    allowed = true;
                }
            } else if (table == QStringLiteral("charging_piles")) {
                if (where.isEmpty() && bind.isEmpty()) {
                    rows = m_ds->query(table);
                    allowed = true;
                } else if ((where == QStringLiteral("id = ?")
                            || where == QStringLiteral("station_id = ?"))
                           && bind.size() == 1) {
                    rows = m_ds->query(table, {}, where, bind);
                    allowed = true;
                }
            } else if (table == QStringLiteral("users")
                       && where == QStringLiteral("id = ?") && bind.size() == 1) {
                rows = m_ds->query(table, {}, QStringLiteral("id = ?"),
                                   {session.userId});
                allowed = true;
            } else if (table == QStringLiteral("orders")) {
                if (where == QStringLiteral("user_id = ?") && bind.size() == 1) {
                    rows = m_ds->query(table, {}, QStringLiteral("user_id = ?"),
                                       {session.userId});
                    allowed = true;
                } else if (where == QStringLiteral(
                               "user_id = ? AND status = '充电中'")
                           && bind.size() == 1) {
                    rows = m_ds->query(table, {}, QStringLiteral(
                        "user_id = ? AND status = '充电中'"), {session.userId});
                    allowed = true;
                } else if (where == QStringLiteral("id = ?") && bind.size() == 1) {
                    rows = m_ds->query(table, {},
                                       QStringLiteral("id = ? AND user_id = ?"),
                                       {bind.first(), session.userId});
                    allowed = true;
                }
            }
            if (allowed)
                setRows(rows);
            else
                setError(QStringLiteral("无权执行该查询"));
        } else if (op == QStringLiteral("update") && table == QStringLiteral("users")) {
            QHash<QString, QVariant> updates = values();
            bool allowed = !updates.isEmpty();
            for (auto it = updates.cbegin(); it != updates.cend(); ++it) {
                if (it.key() != QStringLiteral("nickname")
                    && it.key() != QStringLiteral("avatar")) {
                    allowed = false;
                    break;
                }
            }
            if (!allowed) {
                setError(QStringLiteral("用户只能修改自己的昵称和头像"));
            } else {
                const int affected = m_ds->updateRows(
                    table, updates, QStringLiteral("id = ?"), {session.userId});
                resp.insert(QStringLiteral("ok"), true);
                resp.insert(QStringLiteral("data"), affected);
            }
        } else if (op == QStringLiteral("rechargeBalance")) {
            const bool ok = m_ds->rechargeBalance(
                session.userId, values().value(QStringLiteral("amount")).toDouble());
            resp.insert(QStringLiteral("ok"), ok);
            if (ok)
                resp.insert(QStringLiteral("data"), true);
            else
                resp.insert(QStringLiteral("error"), QStringLiteral("充值金额无效"));
        } else if (op == QStringLiteral("startCharge")) {
            const qlonglong id = m_ds->startCharge(
                session.userId, values().value(QStringLiteral("pileId")).toLongLong());
            resp.insert(QStringLiteral("ok"), id > 0);
            if (id > 0)
                resp.insert(QStringLiteral("data"), QJsonValue(id));
            else
                resp.insert(QStringLiteral("error"),
                            QStringLiteral("用户有尚未完成的订单或充电桩不可用"));
        } else if (op == QStringLiteral("updateChargingProgress")) {
            const qlonglong orderId =
                values().value(QStringLiteral("orderId")).toLongLong();
            const QueryResult ownOrder = m_ds->query(
                QStringLiteral("orders"), {QStringLiteral("id")},
                QStringLiteral("id = ? AND user_id = ? AND status = '充电中'"),
                {orderId, session.userId});
            const bool ok = !ownOrder.isEmpty()
                            && m_ds->updateChargingProgress(orderId);
            resp.insert(QStringLiteral("ok"), true);
            resp.insert(QStringLiteral("data"), ok);
        } else if (op == QStringLiteral("stopCharge")) {
            const qlonglong orderId =
                values().value(QStringLiteral("orderId")).toLongLong();
            const QueryResult ownOrder = m_ds->query(
                QStringLiteral("orders"), {QStringLiteral("id")},
                QStringLiteral("id = ? AND user_id = ? AND status = '充电中'"),
                {orderId, session.userId});
            const bool ok = !ownOrder.isEmpty() && m_ds->stopCharge(orderId);
            resp.insert(QStringLiteral("ok"), ok);
            if (ok)
                resp.insert(QStringLiteral("data"), true);
            else
                resp.insert(QStringLiteral("error"), QStringLiteral("订单无法停止"));
        } else if (op == QStringLiteral("settleCharge")) {
            const auto requestValues = values();
            const qlonglong orderId =
                requestValues.value(QStringLiteral("orderId")).toLongLong();
            const QueryResult ownOrder = m_ds->query(
                QStringLiteral("orders"), {QStringLiteral("id")},
                QStringLiteral("id = ? AND user_id = ? AND status = '待支付'"),
                {orderId, session.userId});
            const bool ok = !ownOrder.isEmpty() && m_ds->settleCharge(orderId);
            resp.insert(QStringLiteral("ok"), ok);
            if (ok)
                resp.insert(QStringLiteral("data"), true);
            else
                resp.insert(QStringLiteral("error"),
                            QStringLiteral("订单不存在、不属于当前用户、非待支付状态或余额不足"));
        } else {
            setError(QStringLiteral("用户无权执行该操作"));
        }
        sendToClient(client, resp);
        return;
    }

    // 管理员只管理业务数据。账号凭据不通过通用接口暴露，充电/结算只属于用户会话。
    if (op == QStringLiteral("startCharge")
        || op == QStringLiteral("updateChargingProgress")
        || op == QStringLiteral("stopCharge")
        || op == QStringLiteral("settleCharge")
        || op == QStringLiteral("rechargeBalance")) {
        setError(QStringLiteral("管理员无权执行用户业务"));
        sendToClient(client, resp);
        return;
    }
    static const QSet<QString> readableTables = {
        QStringLiteral("users"), QStringLiteral("charging_stations"),
        QStringLiteral("charging_piles"), QStringLiteral("orders")
    };
    static const QSet<QString> insertableTables = {
        QStringLiteral("charging_stations"), QStringLiteral("charging_piles")
    };
    static const QSet<QString> updateableTables = {
        QStringLiteral("users"), QStringLiteral("charging_stations"),
        QStringLiteral("charging_piles")
    };
    if ((op == QStringLiteral("query") && !readableTables.contains(table))
        || (op == QStringLiteral("insert") && !insertableTables.contains(table))
        || (op == QStringLiteral("update") && !updateableTables.contains(table))
        || op == QStringLiteral("remove")) {
        setError(QStringLiteral("无权通过通用接口操作该表"));
        sendToClient(client, resp);
        return;
    }

    if (op == QStringLiteral("restartChargingPile")) {
        const QHash<QString, QVariant> values =
            Protocol::jsonToValues(msg.value(QStringLiteral("values")).toObject());
        const bool ok = m_ds->restartChargingPile(
            values.value(QStringLiteral("pileId")).toLongLong());
        resp.insert(QStringLiteral("ok"), ok);
        if (ok)
            resp.insert(QStringLiteral("data"), true);
        else
            resp.insert(QStringLiteral("error"),
                        QStringLiteral("电桩不存在、正在使用或重启失败"));
    } else if (op == QStringLiteral("removeChargingPile")) {
        const QHash<QString, QVariant> values =
            Protocol::jsonToValues(msg.value(QStringLiteral("values")).toObject());
        const bool ok = m_ds->removeChargingPile(
            values.value(QStringLiteral("pileId")).toLongLong());
        resp.insert(QStringLiteral("ok"), ok);
        if (ok)
            resp.insert(QStringLiteral("data"), true);
        else
            resp.insert(QStringLiteral("error"),
                        QStringLiteral("电桩不存在、正在使用或已有订单记录"));
    } else if (op == QStringLiteral("removeChargingStation")) {
        const QHash<QString, QVariant> values =
            Protocol::jsonToValues(msg.value(QStringLiteral("values")).toObject());
        const bool ok = m_ds->removeChargingStation(
            values.value(QStringLiteral("stationId")).toLongLong());
        resp.insert(QStringLiteral("ok"), ok);
        if (ok)
            resp.insert(QStringLiteral("data"), true);
        else
            resp.insert(QStringLiteral("error"),
                        QStringLiteral("电站不存在，或其电桩正在使用/已有订单记录"));
    } else if (op == QStringLiteral("query")) {
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
    } else {
        resp.insert(QStringLiteral("ok"), QJsonValue(false));
        resp.insert(QStringLiteral("error"), QStringLiteral("unknown op: ") + op);
    }

    sendToClient(client, resp);
}
