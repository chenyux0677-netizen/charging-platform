#include "TableDef.h"

#include <QStringList>

QString TableDef::createTableSql() const
{
    QStringList colDefs;
    for (const ColumnDef &c : columns) {
        QString d = c.name + QLatin1Char(' ') + c.sqlType;
        if (c.primaryKey)
            d += QStringLiteral(" PRIMARY KEY");
        if (c.autoIncrement)
            d += QStringLiteral(" AUTOINCREMENT");
        if (c.notNull)
            d += QStringLiteral(" NOT NULL");
        if (c.unique)
            d += QStringLiteral(" UNIQUE");
        if (!c.defaultValue.isEmpty())
            d += QStringLiteral(" DEFAULT ") + c.defaultValue;
        colDefs << d;
    }
    colDefs << constraints;
    return QStringLiteral("CREATE TABLE IF NOT EXISTS %1 (\n    %2\n);")
        .arg(tableName, colDefs.join(QStringLiteral(",\n    ")));
}

QVector<TableDef> allTableDefs()
{
    QVector<TableDef> defs;

    // ===== 用户表(兼作手机号免密登录账号) =====
    TableDef users;
    users.tableName = QStringLiteral("users");
    users.columns = {
        ColumnDef(QStringLiteral("id"),         QStringLiteral("INTEGER"), true,  true),
        ColumnDef(QStringLiteral("phone"),      QStringLiteral("TEXT"),    false, false, true, true),
        ColumnDef(QStringLiteral("nickname"),   QStringLiteral("TEXT"),    false, false, true),
        ColumnDef(QStringLiteral("avatar"),     QStringLiteral("TEXT"),    false, false, false, false, QStringLiteral("''")),
        ColumnDef(QStringLiteral("balance"),    QStringLiteral("REAL"),    false, false, false, false, QStringLiteral("0")),
        ColumnDef(QStringLiteral("status"),     QStringLiteral("TEXT"),    false, false, false, false, QStringLiteral("'正常'")),
        ColumnDef(QStringLiteral("created_at"), QStringLiteral("TEXT"),    false, false, false, false, QStringLiteral("(datetime('now','localtime'))")),
    };
    users.constraints = {
        QStringLiteral("CHECK (status IN ('正常', '冻结'))"),
        QStringLiteral("CHECK (balance >= 0)")
    };
    defs << users;

    // ===== 充电站表 =====
    TableDef stations;
    stations.tableName = QStringLiteral("charging_stations");
    stations.columns = {
        ColumnDef(QStringLiteral("id"),            QStringLiteral("INTEGER"), true,  true),
        ColumnDef(QStringLiteral("name"),          QStringLiteral("TEXT"),    false, false, true),
        ColumnDef(QStringLiteral("address"),       QStringLiteral("TEXT"),    false, false, true),
        ColumnDef(QStringLiteral("lat"),           QStringLiteral("REAL")),
        ColumnDef(QStringLiteral("lng"),           QStringLiteral("REAL")),
    };
    stations.constraints = {
        QStringLiteral("CHECK (lat IS NULL OR lat BETWEEN -90 AND 90)"),
        QStringLiteral("CHECK (lng IS NULL OR lng BETWEEN -180 AND 180)")
    };
    defs << stations;

    // ===== 充电桩表(从属于充电站) =====
    TableDef piles;
    piles.tableName = QStringLiteral("charging_piles");
    piles.columns = {
        ColumnDef(QStringLiteral("id"),                  QStringLiteral("INTEGER"), true,  true),
        ColumnDef(QStringLiteral("station_id"),          QStringLiteral("INTEGER"), false, false, true),
        ColumnDef(QStringLiteral("code"),                QStringLiteral("TEXT"),    false, false, true),
        ColumnDef(QStringLiteral("type"),                QStringLiteral("TEXT"),    false, false, true),
        ColumnDef(QStringLiteral("power_kw"),            QStringLiteral("REAL"),    false, false, true),
        ColumnDef(QStringLiteral("price_per_kwh"),       QStringLiteral("REAL"),    false, false, true),
        ColumnDef(QStringLiteral("status"),              QStringLiteral("TEXT"),    false, false, false, false, QStringLiteral("'空闲'")),
        ColumnDef(QStringLiteral("charge_count"),        QStringLiteral("INTEGER"), false, false, false, false, QStringLiteral("0")),
        ColumnDef(QStringLiteral("charge_duration_min"), QStringLiteral("INTEGER"), false, false, false, false, QStringLiteral("0")),
    };
    piles.constraints = {
        QStringLiteral("FOREIGN KEY (station_id) REFERENCES charging_stations(id) ON DELETE RESTRICT"),
        QStringLiteral("UNIQUE (station_id, code)"),
        QStringLiteral("CHECK (power_kw > 0)"),
        QStringLiteral("CHECK (price_per_kwh > 0)"),
        QStringLiteral("CHECK (status IN ('空闲', '使用中', '故障'))"),
        QStringLiteral("CHECK (charge_count >= 0)"),
        QStringLiteral("CHECK (charge_duration_min >= 0)")
    };
    defs << piles;

    // ===== 订单表(用户充电核心) =====
    TableDef orders;
    orders.tableName = QStringLiteral("orders");
    orders.columns = {
        ColumnDef(QStringLiteral("id"),         QStringLiteral("INTEGER"), true,  true),
        ColumnDef(QStringLiteral("user_id"),    QStringLiteral("INTEGER"), false, false, true),
        ColumnDef(QStringLiteral("pile_id"),    QStringLiteral("INTEGER"), false, false, true),
        ColumnDef(QStringLiteral("start_time"), QStringLiteral("TEXT"),    false, false, true),
        ColumnDef(QStringLiteral("end_time"),   QStringLiteral("TEXT")),
        ColumnDef(QStringLiteral("duration_min"), QStringLiteral("INTEGER"), false, false, true, false, QStringLiteral("0")),
        ColumnDef(QStringLiteral("energy_kwh"), QStringLiteral("REAL"),    false, false, false, false, QStringLiteral("0")),
        ColumnDef(QStringLiteral("amount"),     QStringLiteral("REAL"),    false, false, false, false, QStringLiteral("0")),
        ColumnDef(QStringLiteral("status"),     QStringLiteral("TEXT"),    false, false, false, false, QStringLiteral("'充电中'")),
    };
    orders.constraints = {
        QStringLiteral("FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE RESTRICT"),
        QStringLiteral("FOREIGN KEY (pile_id) REFERENCES charging_piles(id) ON DELETE RESTRICT"),
        QStringLiteral("CHECK (energy_kwh >= 0)"),
        QStringLiteral("CHECK (amount >= 0)"),
        QStringLiteral("CHECK (duration_min >= 0)"),
        QStringLiteral("CHECK (status IN ('充电中', '待支付', '已完成'))")
    };
    orders.indexes = {
        QStringLiteral("CREATE UNIQUE INDEX IF NOT EXISTS idx_orders_active_user "
                       "ON orders(user_id) WHERE status IN ('充电中', '待支付')"),
        QStringLiteral("CREATE UNIQUE INDEX IF NOT EXISTS idx_orders_active_pile "
                       "ON orders(pile_id) WHERE status = '充电中'")
    };
    defs << orders;

    // ===== 管理员表(账号密码登录) =====
    TableDef admins;
    admins.tableName = QStringLiteral("admins");
    admins.columns = {
        ColumnDef(QStringLiteral("id"),            QStringLiteral("INTEGER"), true,  true),
        ColumnDef(QStringLiteral("username"),      QStringLiteral("TEXT"),    false, false, true, true),
        ColumnDef(QStringLiteral("password_salt"), QStringLiteral("TEXT"),    false, false, true),
        ColumnDef(QStringLiteral("password_hash"), QStringLiteral("TEXT"),    false, false, true),
    };
    defs << admins;

    return defs;
}
