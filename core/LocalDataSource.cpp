#include "LocalDataSource.h"
#include "TableDef.h"

#include <QDateTime>
#include <QDebug>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

namespace {
// 本机唯一的命名连接,避免重复注册报警
const QString kConnName = QStringLiteral("charging_local");

// 折算口径唯一出处:结算(settleCharge)与充电中实时进度(updateChargingProgress)共用,
// 避免两处各自实现导致口径漂移。演示口径 1 秒 = 1 分钟:
// minutes = 开始时间到现在经过的秒数(下限 1),energy = 功率×时长/60,amount = 电量×单价。
void computeChargeMetrics(const QDateTime &start, double powerKw, double pricePerKwh,
                          qlonglong &minutes, double &energy, double &amount)
{
    minutes = qMax<qlonglong>(1, start.secsTo(QDateTime::currentDateTime()));
    energy = powerKw * minutes / 60.0;
    amount = energy * pricePerKwh;
}
}

LocalDataSource::LocalDataSource(QObject *parent)
    : DataSource(parent)
{
}

LocalDataSource::~LocalDataSource()
{
    m_db = QSqlDatabase(); // 先释放内部引用
    QSqlDatabase::removeDatabase(kConnName);
}

bool LocalDataSource::open(const QString &dbPath)
{
    m_dbPath = dbPath;
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), kConnName);
    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) {
        qWarning() << "[LocalDataSource] 打开数据库失败:" << m_db.lastError().text();
        return false;
    }
    // 外键约束(桩→站、订单→桩/用户)在本连接上生效;需在无事务时启用
    {
        QSqlQuery foreignKeys(m_db);
        if (!foreignKeys.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
            qWarning() << "[LocalDataSource] 启用外键失败:"
                       << foreignKeys.lastError().text();
            return false;
        }
    }
    if (!createTablesIfNeeded()) {
        return false;
    }
    if (!recoverChargingState()) {
        return false;
    }
    m_open = true;
    qInfo() << "[LocalDataSource] 数据库已就绪:" << dbPath;
    return true;
}

bool LocalDataSource::isOpen() const
{
    return m_open;
}

QString LocalDataSource::dbPath() const
{
    return m_dbPath;
}

bool LocalDataSource::createTablesIfNeeded()
{
    for (const TableDef &def : allTableDefs()) {
        QSqlQuery q(m_db);
        if (!q.exec(def.createTableSql())) {
            qWarning() << "[LocalDataSource] 建表失败:" << def.tableName
                       << "->" << q.lastError().text();
            return false;
        }
        qInfo() << "[LocalDataSource] 表已就绪:" << def.tableName;
    }
    // CREATE 只保证表存在;旧版本生成的库会缺新版新增的列,这里自动补齐(见 migrateMissingColumns)
    if (!migrateMissingColumns())
        return false;

    // 建表/补列之后创建表定义里声明的索引(orders 的部分唯一索引)。
    // IF NOT EXISTS 保证重复启动幂等;新库此刻表为空,必然成功。
    for (const TableDef &def : allTableDefs()) {
        for (const QString &sql : def.indexes) {
            QSqlQuery q(m_db);
            if (!q.exec(sql)) {
                qWarning() << "[LocalDataSource] 索引创建失败:" << q.lastError().text()
                           << "| SQL:" << sql;
                return false;
            }
        }
    }
    return true;
}

bool LocalDataSource::migrateMissingColumns()
{
    for (const TableDef &def : allTableDefs()) {
        // 读出表现有的列名集合
        QSet<QString> existing;
        {
            QSqlQuery qCol(m_db);
            if (!qCol.exec(QStringLiteral("PRAGMA table_info(%1)").arg(def.tableName))) {
                qWarning() << "[LocalDataSource] 读取列信息失败:" << def.tableName
                           << "->" << qCol.lastError().text();
                return false;
            }
            // PRAGMA table_info 每行: [cid, name, type, notnull, dflt_value, pk]
            while (qCol.next())
                existing.insert(qCol.value(1).toString());
        }

        for (const ColumnDef &col : def.columns) {
            if (existing.contains(col.name))
                continue;

            // SQLite 的 ALTER TABLE ADD COLUMN 不能带主键/自增/唯一约束
            if (col.primaryKey || col.autoIncrement || col.unique) {
                qWarning() << "[LocalDataSource] 列" << def.tableName << "." << col.name
                           << "带主键/自增/唯一约束,无法自动补充,需删旧库重建(当前代码不会新增此类列)";
                continue;
            }

            // ADD COLUMN 的 DEFAULT 必须是字面量,带括号的表达式(如 datetime(...))不被允许
            const bool constantDefault = !col.defaultValue.isEmpty()
                && !col.defaultValue.startsWith(QLatin1Char('('));

            QString ddl = QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3")
                              .arg(def.tableName, col.name, col.sqlType);
            if (col.notNull && constantDefault) {
                ddl += QStringLiteral(" NOT NULL DEFAULT %1").arg(col.defaultValue);
            } else if (!col.notNull && constantDefault) {
                ddl += QStringLiteral(" DEFAULT %1").arg(col.defaultValue);
            } else if (col.notNull) {
                // 要求 NOT NULL 但拿不到字面量默认值:SQLite 不允许,降级为可空列
                // (旧行与依赖数据库默认值的新行此列可能为 NULL,但不影响读写正常)
                qWarning() << "[LocalDataSource] 列" << def.tableName << "." << col.name
                           << "要求 NOT NULL 但默认值非常量,已按可空列补充";
            }

            QSqlQuery qAdd(m_db);
            if (!qAdd.exec(ddl)) {
                qWarning() << "[LocalDataSource] 自动补列失败:" << def.tableName << "." << col.name
                           << "->" << qAdd.lastError().text() << "| SQL:" << ddl;
                return false;
            }
            qInfo() << "[LocalDataSource] 旧库自动补列:" << def.tableName << "+" << col.name;
        }
    }
    return true;
}

QueryResult LocalDataSource::query(const QString &table,
                                   const QStringList &fields,
                                   const QString &where,
                                   const QVariantList &bindValues)
{
    QueryResult result;
    if (!m_db.isOpen())
        return result;

    const QString fieldSql = fields.isEmpty()
        ? QStringLiteral("*")
        : fields.join(QStringLiteral(", "));
    QString sql = QStringLiteral("SELECT %1 FROM %2").arg(fieldSql, table);
    if (!where.isEmpty())
        sql += QStringLiteral(" WHERE ") + where;

    QSqlQuery q(m_db);
    q.prepare(sql);
    for (const QVariant &v : bindValues)
        q.addBindValue(v);

    if (!q.exec()) {
        qWarning() << "[LocalDataSource] 查询失败:" << q.lastError().text()
                   << "| SQL:" << sql;
        return result;
    }

    const QSqlRecord rec = q.record();
    while (q.next()) {
        DataRow row;
        for (int i = 0; i < rec.count(); ++i)
            row.insert(rec.fieldName(i), q.value(i));
        result << row;
    }
    return result;
}

qlonglong LocalDataSource::insertRow(const QString &table,
                                     const QHash<QString, QVariant> &values)
{
    if (!m_db.isOpen() || values.isEmpty())
        return -1;

    const QStringList cols = values.keys();
    const QString colSql = cols.join(QStringLiteral(", "));
    QStringList placeholders;
    for (int i = 0; i < cols.size(); ++i)
        placeholders << QStringLiteral("?");

    const QString sql = QStringLiteral("INSERT INTO %1 (%2) VALUES (%3)")
                            .arg(table, colSql, placeholders.join(QStringLiteral(", ")));

    QSqlQuery q(m_db);
    q.prepare(sql);
    for (const QString &c : cols)
        q.addBindValue(values.value(c));

    if (!q.exec()) {
        qWarning() << "[LocalDataSource] 插入失败:" << q.lastError().text()
                   << "| SQL:" << sql;
        return -1;
    }

    const qlonglong newId = q.lastInsertId().toLongLong();
    emit dataChanged(table);
    return newId;
}

int LocalDataSource::updateRows(const QString &table,
                                const QHash<QString, QVariant> &values,
                                const QString &where,
                                const QVariantList &bindValues)
{
    if (!m_db.isOpen() || values.isEmpty() || where.isEmpty())
        return 0;

    QStringList sets;
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        sets << (it.key() + QStringLiteral(" = ?"));
    const QString sql = QStringLiteral("UPDATE %1 SET %2 WHERE %3")
                            .arg(table, sets.join(QStringLiteral(", ")), where);

    QSqlQuery q(m_db);
    q.prepare(sql);
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        q.addBindValue(it.value());
    for (const QVariant &v : bindValues)
        q.addBindValue(v);

    if (!q.exec()) {
        qWarning() << "[LocalDataSource] 更新失败:" << q.lastError().text()
                   << "| SQL:" << sql;
        return 0;
    }

    const int affected = q.numRowsAffected();
    emit dataChanged(table);
    return affected;
}

int LocalDataSource::removeRows(const QString &table,
                                const QString &where,
                                const QVariantList &bindValues)
{
    if (!m_db.isOpen() || where.isEmpty())
        return 0;

    const QString sql = QStringLiteral("DELETE FROM %1 WHERE %2").arg(table, where);

    QSqlQuery q(m_db);
    q.prepare(sql);
    for (const QVariant &v : bindValues)
        q.addBindValue(v);

    if (!q.exec()) {
        qWarning() << "[LocalDataSource] 删除失败:" << q.lastError().text()
                   << "| SQL:" << sql;
        return 0;
    }

    const int affected = q.numRowsAffected();
    emit dataChanged(table);
    return affected;
}

// ---- 业务级接口:多表写入都在单事务里完成,失败整体回滚 ----

bool LocalDataSource::recoverChargingState()
{
    if (!m_db.transaction())
        return false;

    // 进行中订单是恢复依据:它对应的桩必须保持"使用中"。
    QSqlQuery restoreBusy(m_db);
    if (!restoreBusy.exec(QStringLiteral(
            "UPDATE charging_piles SET status = '使用中' "
            "WHERE EXISTS (SELECT 1 FROM orders "
            "WHERE orders.pile_id = charging_piles.id AND orders.status = '充电中') "
            "AND status <> '使用中'"))) {
        qWarning() << "[LocalDataSource] 恢复占用电桩失败:"
                   << restoreBusy.lastError().text();
        m_db.rollback();
        return false;
    }

    // 没有进行中订单却仍标记"使用中",说明上次异常退出前状态已不一致。
    QSqlQuery releaseStale(m_db);
    if (!releaseStale.exec(QStringLiteral(
            "UPDATE charging_piles SET status = '空闲' "
            "WHERE status = '使用中' AND NOT EXISTS "
            "(SELECT 1 FROM orders WHERE orders.pile_id = charging_piles.id "
            "AND orders.status = '充电中')"))) {
        qWarning() << "[LocalDataSource] 释放异常占用电桩失败:"
                   << releaseStale.lastError().text();
        m_db.rollback();
        return false;
    }

    if (!m_db.commit()) {
        m_db.rollback();
        return false;
    }
    const int restored = restoreBusy.numRowsAffected();
    const int released = releaseStale.numRowsAffected();
    if (restored > 0 || released > 0)
        qInfo() << "[LocalDataSource] 启动恢复完成:恢复占用" << restored
                << "个,释放异常占用" << released << "个";
    return true;
}

qlonglong LocalDataSource::startCharge(qlonglong userId, qlonglong pileId)
{
    if (!m_db.isOpen() || userId <= 0 || pileId <= 0)
        return -1;
    if (!m_db.transaction()) {
        qWarning() << "[LocalDataSource] 开始充电事务启动失败:"
                   << m_db.lastError().text();
        return -1;
    }

    // 冻结/不存在的用户不能开始新订单。
    QSqlQuery user(m_db);
    user.prepare(QStringLiteral("SELECT 1 FROM users WHERE id = ? AND status = '正常'"));
    user.addBindValue(userId);
    if (!user.exec() || !user.next()) {
        m_db.rollback();
        return -1;
    }

    // 一个用户同时只能有一笔进行中订单。
    QSqlQuery active(m_db);
    active.prepare(QStringLiteral(
        "SELECT 1 FROM orders WHERE user_id = ? AND status = '充电中' LIMIT 1"));
    active.addBindValue(userId);
    if (!active.exec() || active.next()) {
        m_db.rollback();
        return -1;
    }

    // 条件更新就是"抢桩":只有仍为空闲的桩能被一个请求成功占用。
    QSqlQuery claim(m_db);
    claim.prepare(QStringLiteral(
        "UPDATE charging_piles SET status = '使用中' WHERE id = ? AND status = '空闲'"));
    claim.addBindValue(pileId);
    if (!claim.exec() || claim.numRowsAffected() != 1) {
        m_db.rollback();
        return -1;
    }

    QSqlQuery order(m_db);
    order.prepare(QStringLiteral(
        "INSERT INTO orders (user_id, pile_id, start_time, status) "
        "VALUES (?, ?, ?, '充电中')"));
    order.addBindValue(userId);
    order.addBindValue(pileId);
    order.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    if (!order.exec()) {
        qWarning() << "[LocalDataSource] 创建充电订单失败:" << order.lastError().text();
        m_db.rollback();
        return -1;
    }
    const qlonglong orderId = order.lastInsertId().toLongLong();

    if (!m_db.commit()) {
        qWarning() << "[LocalDataSource] 开始充电事务提交失败:"
                   << m_db.lastError().text();
        m_db.rollback();
        return -1;
    }
    emit dataChanged(QStringLiteral("charging_piles"));
    emit dataChanged(QStringLiteral("orders"));
    return orderId;
}

bool LocalDataSource::settleCharge(qlonglong orderId)
{
    if (!m_db.isOpen() || orderId <= 0)
        return false;
    if (!m_db.transaction()) {
        qWarning() << "[LocalDataSource] 结算事务启动失败:"
                   << m_db.lastError().text();
        return false;
    }

    QSqlQuery order(m_db);
    order.prepare(QStringLiteral(
        "SELECT user_id, pile_id, start_time FROM orders "
        "WHERE id = ? AND status = '充电中'"));
    order.addBindValue(orderId);
    if (!order.exec() || !order.next()) {
        m_db.rollback();
        return false;
    }
    const qlonglong userId = order.value(0).toLongLong();
    const qlonglong pileId = order.value(1).toLongLong();
    const QDateTime start = QDateTime::fromString(order.value(2).toString(), Qt::ISODate);

    QSqlQuery pile(m_db);
    pile.prepare(QStringLiteral(
        "SELECT power_kw, price_per_kwh FROM charging_piles WHERE id = ?"));
    pile.addBindValue(pileId);
    if (!pile.exec() || !pile.next()) {
        m_db.rollback();
        return false;
    }

    if (!start.isValid()) {
        m_db.rollback();
        return false;
    }
    // 演示口径 1 秒 = 1 分钟:时长由服务器按"开始时间 → 当前时间"折算(与实时进度同口径)
    qlonglong minutes = 0;
    double energy = 0.0;
    double amount = 0.0;
    computeChargeMetrics(start, pile.value(0).toDouble(), pile.value(1).toDouble(),
                         minutes, energy, amount);
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);

    // status 条件保证同一订单只会被成功结算一次;同时把最终时长落库(duration_min)。
    QSqlQuery finish(m_db);
    finish.prepare(QStringLiteral(
        "UPDATE orders SET end_time = ?, duration_min = ?, energy_kwh = ?, "
        "amount = ?, status = '已完成' WHERE id = ? AND status = '充电中'"));
    finish.addBindValue(now);
    finish.addBindValue(minutes);
    finish.addBindValue(energy);
    finish.addBindValue(amount);
    finish.addBindValue(orderId);
    if (!finish.exec() || finish.numRowsAffected() != 1) {
        m_db.rollback();
        return false;
    }

    QSqlQuery release(m_db);
    release.prepare(QStringLiteral(
        "UPDATE charging_piles SET status = '空闲', "
        "charge_count = charge_count + 1, "
        "charge_duration_min = charge_duration_min + ? WHERE id = ?"));
    release.addBindValue(minutes);
    release.addBindValue(pileId);
    if (!release.exec() || release.numRowsAffected() != 1) {
        m_db.rollback();
        return false;
    }

    // 余额守卫:不足则整单回滚(订单保持"充电中",充值后可重试)
    QSqlQuery debit(m_db);
    debit.prepare(QStringLiteral(
        "UPDATE users SET balance = balance - ? WHERE id = ? AND balance >= ?"));
    debit.addBindValue(amount);
    debit.addBindValue(userId);
    debit.addBindValue(amount);
    if (!debit.exec() || debit.numRowsAffected() != 1) {
        m_db.rollback();
        return false;
    }

    if (!m_db.commit()) {
        qWarning() << "[LocalDataSource] 结算事务提交失败:" << m_db.lastError().text();
        m_db.rollback();
        return false;
    }
    emit dataChanged(QStringLiteral("orders"));
    emit dataChanged(QStringLiteral("charging_piles"));
    emit dataChanged(QStringLiteral("users"));
    qInfo() << "[LocalDataSource] 订单" << orderId << "事务结算完成:"
            << QStringLiteral("%1 分钟, %2 kWh, %3 元")
                   .arg(minutes)
                   .arg(energy, 0, 'f', 2)
                   .arg(amount, 0, 'f', 2);
    return true;
}

bool LocalDataSource::updateChargingProgress(qlonglong orderId)
{
    if (!m_db.isOpen() || orderId <= 0)
        return false;

    // 只认"充电中"订单:已结算 / 不存在直接返回,不 emit。
    QSqlQuery order(m_db);
    order.prepare(QStringLiteral(
        "SELECT pile_id, start_time FROM orders WHERE id = ? AND status = '充电中'"));
    order.addBindValue(orderId);
    if (!order.exec() || !order.next())
        return false;
    const qlonglong pileId = order.value(0).toLongLong();
    const QDateTime start = QDateTime::fromString(order.value(1).toString(), Qt::ISODate);

    QSqlQuery pile(m_db);
    pile.prepare(QStringLiteral(
        "SELECT power_kw, price_per_kwh FROM charging_piles WHERE id = ?"));
    pile.addBindValue(pileId);
    if (!pile.exec() || !pile.next() || !start.isValid())
        return false;

    qlonglong minutes = 0;
    double energy = 0.0;
    double amount = 0.0;
    computeChargeMetrics(start, pile.value(0).toDouble(), pile.value(1).toDouble(),
                         minutes, energy, amount);

    // 单条 UPDATE 本身原子;WHERE status='充电中' 兜底并发/与结算交错:
    // 订单若已被 settleCharge 置为"已完成",这里影响 0 行 → 返回 false,不覆盖终态。
    QSqlQuery up(m_db);
    up.prepare(QStringLiteral(
        "UPDATE orders SET duration_min = ?, energy_kwh = ?, amount = ? "
        "WHERE id = ? AND status = '充电中'"));
    up.addBindValue(minutes);
    up.addBindValue(energy);
    up.addBindValue(amount);
    up.addBindValue(orderId);
    if (!up.exec() || up.numRowsAffected() != 1)
        return false;

    emit dataChanged(QStringLiteral("orders")); // Server 据此广播 → 管理员端实时刷新
    return true;
}

bool LocalDataSource::rechargeBalance(qlonglong userId, double amount)
{
    if (!m_db.isOpen() || userId <= 0 || !qIsFinite(amount)
        || amount < 0.01 || amount > 1000000.0)
        return false;
    // 只在用户状态正常时加钱,冻结/不存在的用户不响应
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE users SET balance = balance + ? WHERE id = ? AND status = '正常'"));
    q.addBindValue(amount);
    q.addBindValue(userId);
    if (!q.exec() || q.numRowsAffected() != 1)
        return false;
    emit dataChanged(QStringLiteral("users"));
    return true;
}

bool LocalDataSource::removeChargingPile(qlonglong pileId)
{
    if (!m_db.isOpen() || pileId <= 0 || !m_db.transaction())
        return false;

    // 使用中的桩、或任何有过订单记录的桩都不能删(外键亦会拒绝)
    QSqlQuery remove(m_db);
    remove.prepare(QStringLiteral(
        "DELETE FROM charging_piles WHERE id = ? AND status <> '使用中' "
        "AND NOT EXISTS (SELECT 1 FROM orders WHERE pile_id = ?)"));
    remove.addBindValue(pileId);
    remove.addBindValue(pileId);
    if (!remove.exec() || remove.numRowsAffected() != 1) {
        m_db.rollback();
        return false;
    }
    if (!m_db.commit()) {
        m_db.rollback();
        return false;
    }
    emit dataChanged(QStringLiteral("charging_piles"));
    return true;
}

bool LocalDataSource::removeChargingStation(qlonglong stationId)
{
    if (!m_db.isOpen() || stationId <= 0 || !m_db.transaction())
        return false;

    // 有使用中电桩或任何历史订单时保留整个电站,避免破坏业务记录
    QSqlQuery protectedPiles(m_db);
    protectedPiles.prepare(QStringLiteral(
        "SELECT 1 FROM charging_piles p WHERE p.station_id = ? AND "
        "(p.status = '使用中' OR EXISTS "
        "(SELECT 1 FROM orders o WHERE o.pile_id = p.id)) LIMIT 1"));
    protectedPiles.addBindValue(stationId);
    if (!protectedPiles.exec() || protectedPiles.next()) {
        m_db.rollback();
        return false;
    }

    QSqlQuery piles(m_db);
    piles.prepare(QStringLiteral("DELETE FROM charging_piles WHERE station_id = ?"));
    piles.addBindValue(stationId);
    if (!piles.exec()) {
        m_db.rollback();
        return false;
    }

    QSqlQuery station(m_db);
    station.prepare(QStringLiteral("DELETE FROM charging_stations WHERE id = ?"));
    station.addBindValue(stationId);
    if (!station.exec() || station.numRowsAffected() != 1) {
        m_db.rollback();
        return false;
    }
    if (!m_db.commit()) {
        m_db.rollback();
        return false;
    }
    emit dataChanged(QStringLiteral("charging_piles"));
    emit dataChanged(QStringLiteral("charging_stations"));
    return true;
}
