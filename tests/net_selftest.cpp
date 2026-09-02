// 通信自检程序:独立小工具,不进入主程序。
// 验证 server/ + common/Protocol + core/RemoteDataSource:
//   一个进程里起 1 个服务器 + 2 个模拟客户端(分别代表两台设备),
//   走真实 TCP + JSON 完成 增删改查,并验证"一端改动 → 另一端实时收到广播"。
#include "core/RemoteDataSource.h"
#include "server/Server.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QTimer>

static int g_fail = 0;

static void check(bool ok, const QString &what)
{
    QTextStream out(stdout);
    out << (ok ? QStringLiteral("  ✓ ") : QStringLiteral("  ✗ ")) << what << "\n";
    if (!ok)
        ++g_fail;
}

// 让事件循环跑一小段,给网络事件(如广播到达)一点处理时间
static void spin(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    out << "=== 充电桩平台 · 通信自检 ===\n\n";

    const QString dbPath = QStringLiteral("server_test.db");
    QFile::remove(dbPath);

    out << "[1/6] 启动服务器\n";
    Server server;
    check(server.start(dbPath, QStringLiteral("127.0.0.1"), 0),
          "服务器启动(端口自动分配)");
    const quint16 actualPort = server.port();
    check(actualPort > 0, QStringLiteral("服务器监听端口 = %1").arg(actualPort));

    out << "\n[2/6] 客户端A 连接并插入用户\n";
    RemoteDataSource clientA;
    check(clientA.connectToServer(QStringLiteral("127.0.0.1"), actualPort),
          "客户端A 连接成功");

    QHash<QString, QVariant> user;
    user.insert(QStringLiteral("phone"), QStringLiteral("13800138000"));
    user.insert(QStringLiteral("nickname"), QStringLiteral("用户8000"));
    user.insert(QStringLiteral("balance"), 100.0);
    const qlonglong uid = clientA.insertRow(QStringLiteral("users"), user);
    check(uid > 0, QStringLiteral("客户端A 经网络插入用户, id = %1").arg(uid));

    out << "\n[3/6] 客户端A 查询\n";
    QueryResult rows = clientA.query(QStringLiteral("users"),
                                     {QStringLiteral("phone"), QStringLiteral("nickname"), QStringLiteral("balance")},
                                     QStringLiteral("id = ?"), QVariantList{uid});
    check(rows.size() == 1, QStringLiteral("客户端A 查到 %1 行").arg(rows.size()));
    if (rows.size() == 1) {
        const DataRow &r = rows.first();
        out << "    查到: phone = " << r.value(QStringLiteral("phone")).toString()
            << ", nickname = " << r.value(QStringLiteral("nickname")).toString()
            << ", balance = " << r.value(QStringLiteral("balance")).toDouble() << "\n";
        check(r.value(QStringLiteral("phone")).toString() == QStringLiteral("13800138000"),
              "手机号正确");
        check(r.value(QStringLiteral("balance")).toDouble() == 100.0, "余额正确(100)");
    }

    out << "\n[4/6] 客户端B 连接(模拟另一台设备)\n";
    RemoteDataSource clientB;
    int notifyCount = 0;
    QString notifiedTable;
    QObject::connect(&clientB, &DataSource::dataChanged, [&](const QString &t) {
        ++notifyCount;
        notifiedTable = t;
    });
    check(clientB.connectToServer(QStringLiteral("127.0.0.1"), actualPort),
          "客户端B 连接成功");

    out << "\n[5/6] 客户端A 改数据 → 服务器广播 → 客户端B 实时收到\n";
    QHash<QString, QVariant> up;
    up.insert(QStringLiteral("balance"), 50.0);
    const int affected = clientA.updateRows(QStringLiteral("users"), up,
                                            QStringLiteral("id = ?"), QVariantList{uid});
    check(affected == 1, "客户端A 更新余额 100→50 成功");
    spin(200); // 给广播一点到达时间
    check(notifyCount >= 1, QStringLiteral("客户端B 收到 %1 次 dataChanged 广播").arg(notifyCount));
    check(notifiedTable == QStringLiteral("users"), "通知的表名 = users");

    out << "\n[6/6] 客户端B 查询更新后的数据(数据共享)\n";
    QueryResult bRows = clientB.query(QStringLiteral("users"),
                                      {QStringLiteral("balance")},
                                      QStringLiteral("id = ?"), QVariantList{uid});
    check(bRows.size() == 1
              && bRows.first().value(QStringLiteral("balance")).toDouble() == 50.0,
          "客户端B 查到余额 = 50(与客户端A 一致)");

    out << "\n=== 自检" << (g_fail == 0 ? QStringLiteral("通过 ✅")
                                       : QStringLiteral("失败(%1 项) ❌").arg(g_fail))
        << " ===\n";
    out << "服务器数据库文件保留在:" << QFileInfo(dbPath).absoluteFilePath() << "\n";

    return g_fail == 0 ? 0 : 1;
}
