// 数据库自检程序:独立小工具,不进入主程序。
// 作用:验证 core/ 数据层 —— 自动建表、增删改查、dataChanged 信号。
// 用法:直接运行可执行文件 db_selftest,看输出是否"自检通过"。
#include "core/DataSource.h"
#include "core/LocalDataSource.h"
#include "core/TableDef.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

static int g_fail = 0;

static void check(bool ok, const QString &what)
{
    QTextStream out(stdout);
    out << (ok ? "  ✓ " : "  ✗ ") << what << "\n";
    if (!ok)
        ++g_fail;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    out << "=== 充电桩平台 · 数据库自检 ===\n\n";

    const QString dbPath = QStringLiteral("selftest.db");
    QFile::remove(dbPath); // 每次从干净状态开始

    LocalDataSource ds;

    out << "[1/5] 打开数据库并自动建表\n";
    check(ds.open(dbPath), QStringLiteral("打开数据库 %1").arg(dbPath));

    out << "\n[2/5] 表结构(CREATE TABLE)\n";
    const QVector<TableDef> defs = allTableDefs();
    check(defs.size() == 5, QStringLiteral("业务表数量 = %1(预期 5)").arg(defs.size()));
    for (const TableDef &d : defs)
        out << "  " << d.createTableSql() << "\n";

    out << "\n[3/5] 插入数据\n";
    QHash<QString, QVariant> user;
    user.insert(QStringLiteral("phone"), QStringLiteral("13800138000"));
    user.insert(QStringLiteral("nickname"), QStringLiteral("用户8000"));
    user.insert(QStringLiteral("balance"), 100.0);
    const qlonglong uid = ds.insertRow(QStringLiteral("users"), user);
    check(uid > 0, QStringLiteral("插入用户成功, id = %1").arg(uid));

    QHash<QString, QVariant> station;
    station.insert(QStringLiteral("name"), QStringLiteral("东软充电站"));
    station.insert(QStringLiteral("address"), QStringLiteral("高新区软件园"));
    station.insert(QStringLiteral("lat"), 38.89);
    station.insert(QStringLiteral("lng"), 121.53);
    const qlonglong sid = ds.insertRow(QStringLiteral("charging_stations"), station);
    check(sid > 0, QStringLiteral("插入充电站成功, id = %1").arg(sid));

    QHash<QString, QVariant> pile;
    pile.insert(QStringLiteral("station_id"), sid);
    pile.insert(QStringLiteral("code"), QStringLiteral("A-01"));
    pile.insert(QStringLiteral("type"), QStringLiteral("快充"));
    pile.insert(QStringLiteral("power_kw"), 60.0);
    pile.insert(QStringLiteral("price_per_kwh"), 1.5);
    const qlonglong pid = ds.insertRow(QStringLiteral("charging_piles"), pile);
    check(pid > 0, QStringLiteral("插入充电桩成功, id = %1").arg(pid));

    out << "\n[4/5] 查询\n";
    QueryResult rows = ds.query(QStringLiteral("users"),
                                {QStringLiteral("phone"), QStringLiteral("nickname"), QStringLiteral("balance")},
                                QStringLiteral("id = ?"),
                                QVariantList{uid});
    check(rows.size() == 1, QStringLiteral("按 id 查到用户 %1 行").arg(rows.size()));
    if (rows.size() == 1) {
        const DataRow &r = rows.first();
        out << "    查到: phone = " << r.value(QStringLiteral("phone")).toString()
            << ", nickname = " << r.value(QStringLiteral("nickname")).toString()
            << ", balance = " << r.value(QStringLiteral("balance")).toDouble() << "\n";
        check(r.value(QStringLiteral("phone")).toString() == QStringLiteral("13800138000"),
              "手机号正确");
        check(r.value(QStringLiteral("balance")).toDouble() == 100.0,
              "余额正确(100)");
    }

    // 跨表:桩属于站
    QueryResult pileRows = ds.query(QStringLiteral("charging_piles"),
                                    {QStringLiteral("code"), QStringLiteral("type"), QStringLiteral("price_per_kwh")},
                                    QStringLiteral("station_id = ?"),
                                    QVariantList{sid});
    check(pileRows.size() == 1
              && pileRows.first().value(QStringLiteral("code")).toString() == QStringLiteral("A-01"),
          "按 station_id 查到所属充电桩 A-01");

    out << "\n[5/5] 更新 + dataChanged 信号\n";
    int signalCount = 0;
    QObject::connect(&ds, &DataSource::dataChanged, [&](const QString &table) {
        if (table == QStringLiteral("users"))
            ++signalCount;
    });

    QHash<QString, QVariant> up;
    up.insert(QStringLiteral("balance"), 50.0);
    const int affected = ds.updateRows(QStringLiteral("users"), up,
                                       QStringLiteral("id = ?"), QVariantList{uid});
    check(affected == 1, QStringLiteral("更新影响 1 行(余额 100 → 50)"));

    QueryResult after = ds.query(QStringLiteral("users"),
                                 {QStringLiteral("balance")},
                                 QStringLiteral("id = ?"), QVariantList{uid});
    check(after.size() == 1
              && after.first().value(QStringLiteral("balance")).toDouble() == 50.0,
          "重新查询余额 = 50");

    check(signalCount == 1, "dataChanged(users) 信号已发出 1 次");

    // 删除
    // 站下还有电桩,裸删充电站被外键(FOREIGN KEY ... ON DELETE RESTRICT)拦下
    const int removed = ds.removeRows(QStringLiteral("charging_stations"),
                                      QStringLiteral("id = ?"), QVariantList{sid});
    check(removed == 0, QStringLiteral("外键拦下直接删除充电站(影响 %1 行,预期 0)").arg(removed));

    // 走安全删除接口:桩空闲且无订单记录 → 服务端在事务里先删桩再删站
    const bool stationRemoved = ds.removeChargingStation(sid);
    check(stationRemoved, "安全删除充电站成功(removeChargingStation)");
    const QueryResult gonePile = ds.query(QStringLiteral("charging_piles"), {},
                                          QStringLiteral("station_id = ?"), QVariantList{sid});
    const QueryResult goneStation = ds.query(QStringLiteral("charging_stations"), {},
                                             QStringLiteral("id = ?"), QVariantList{sid});
    check(gonePile.isEmpty() && goneStation.isEmpty(), "删除后该站的电桩与电站都查不到");

    out << "\n=== 自检" << (g_fail == 0 ? QStringLiteral("通过 ✅") : QStringLiteral("失败(%1 项) ❌").arg(g_fail))
        << " ===\n";
    out << "数据库文件保留在:" << QFileInfo(dbPath).absoluteFilePath() << "\n";
    out << "可用命令查看表结构: sqlite3 " << dbPath << " '.tables'\n";

    return g_fail == 0 ? 0 : 1;
}
