// 界面自检:脚本化模拟真实点击,把 登录链路 完整走一遍:
//   角色选择 → (管理员端) 本机内嵌服务器 + 账号登录 → 管理员主界面
//             (用户端)   手机号自动注册/免密登录     → 用户主界面
// 数据走真实 TCP + SQLite,与主程序共用同一套代码路径。
#include "common/AppContext.h"
#include "common/RoleSelectWindow.h"

#include "admin/AdminLoginWindow.h"
#include "admin/AdminMainWindow.h"
#include "server/Server.h"
#include "user/UserLoginWindow.h"
#include "user/UserMainWindow.h"

#include <QtTest/QtTest>
#include <QApplication>
#include <QDateTime>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTimer>

namespace {
// 关掉测试中会弹出的模态提示框(错误密码/位数不对等),避免阻塞测试。
// 用 reject() 而非 close():保证 QDialog::exec() 一定退出。
void closeModalDialogs()
{
    const QWidgetList top = QApplication::topLevelWidgets();
    for (QWidget *w : top) {
        if (auto *dlg = qobject_cast<QDialog *>(w))
            if (dlg->isVisible())
                dlg->reject();
    }
}

// 从窗口里按 objectName 找控件
template <typename T>
T *find(QWidget *root, const char *name)
{
    return root->findChild<T *>(QLatin1String(name));
}
} // namespace

class GuiFlowTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void roleSelectSignals();
    void adminLoginFlow();
    void userLoginFlow();
    void chargingSeedTest();
    void chargeFlow();
    void unfinishedOrderCheck();

private:
    // 周期守卫:测试中一旦冒出模态提示框,30ms 内自动关掉。
    // (不能用一次性 QTimer::singleShot:登录的网络请求会先耗掉那一拍,
    //  等弹框真出现时定时器已经触发过了。)
    QTimer m_dialogGuard;
};

void GuiFlowTest::initTestCase()
{
    connect(&m_dialogGuard, &QTimer::timeout, this, [] { closeModalDialogs(); });
    m_dialogGuard.start(30);
}

void GuiFlowTest::cleanupTestCase()
{
    m_dialogGuard.stop();
}

// ---- ① 角色选择页:两个按钮发对应信号,并带出服务器地址 ----
void GuiFlowTest::roleSelectSignals()
{
    RoleSelectWindow role;
    role.show();
    QVERIFY(QTest::qWaitForWindowExposed(&role));

    QLineEdit *hostEdit = find<QLineEdit>(&role, "hostEdit");
    QLineEdit *portEdit = find<QLineEdit>(&role, "portEdit");
    QPushButton *userBtn = find<QPushButton>(&role, "userButton");
    QPushButton *adminBtn = find<QPushButton>(&role, "adminButton");
    QVERIFY(hostEdit && portEdit && userBtn && adminBtn);

    // 默认地址应预填
    QCOMPARE(hostEdit->text(), QStringLiteral("127.0.0.1"));
    QCOMPARE(portEdit->text(), QStringLiteral("9527"));

    bool gotUser = false, gotAdmin = false;
    QString gotHost;
    quint16 gotPort = 0;
    connect(&role, &RoleSelectWindow::userModeSelected, this,
            [&](const QString &h, quint16 p) { gotUser = true; gotHost = h; gotPort = p; });
    connect(&role, &RoleSelectWindow::adminModeSelected, this,
            [&] { gotAdmin = true; });

    // 点"用户端" → 带默认地址发出
    QTest::mouseClick(userBtn, Qt::LeftButton);
    QVERIFY(gotUser);
    QCOMPARE(gotHost, QStringLiteral("127.0.0.1"));
    QCOMPARE(gotPort, static_cast<quint16>(9527));

    // 改地址再点 → 带新地址发出
    hostEdit->setText(QStringLiteral("192.168.1.50"));
    gotUser = false;
    QTest::mouseClick(userBtn, Qt::LeftButton);
    QVERIFY(gotUser);
    QCOMPARE(gotHost, QStringLiteral("192.168.1.50"));

    // 点"管理员端" → 发出
    QTest::mouseClick(adminBtn, Qt::LeftButton);
    QVERIFY(gotAdmin);
}

// ---- ② 管理员端:内嵌服务器(含种子账号) + 账号密码登录 ----
void GuiFlowTest::adminLoginFlow()
{
    QFile::remove(QStringLiteral("gui_test_admin.db"));

    // 管理员模式的数据库中间层:内嵌服务器(与 main.cpp 完全同一套)
    Server server;
    QVERIFY(server.start(QStringLiteral("gui_test_admin.db"),
                         QStringLiteral("127.0.0.1"), 0));
    AppContext::instance()->setServer(QStringLiteral("127.0.0.1"), server.port());

    AdminLoginWindow login;
    login.show();
    QVERIFY(QTest::qWaitForWindowExposed(&login));

    QLineEdit *userEdit = find<QLineEdit>(&login, "userEdit");
    QLineEdit *passEdit = find<QLineEdit>(&login, "passEdit");
    QPushButton *loginBtn = find<QPushButton>(&login, "loginButton");
    QVERIFY(userEdit && passEdit && loginBtn);

    // 错误密码 → 弹提示,不发信号
    bool gotSignal = false;
    connect(&login, &AdminLoginWindow::loginSucceeded, this,
            [&](const QString &) { gotSignal = true; });
    userEdit->setText(QStringLiteral("admin"));
    passEdit->setText(QStringLiteral("wrong"));
    QTest::mouseClick(loginBtn, Qt::LeftButton);
    QVERIFY(!gotSignal);

    // 种子账号 admin / 123456 → 成功,带出账号名
    disconnect(&login, &AdminLoginWindow::loginSucceeded, this, nullptr);
    QString gotUsername;
    connect(&login, &AdminLoginWindow::loginSucceeded, this,
            [&](const QString &u) { gotSignal = true; gotUsername = u; });
    passEdit->setText(QStringLiteral("123456"));
    QTest::mouseClick(loginBtn, Qt::LeftButton);
    QVERIFY(gotSignal);
    QCOMPARE(gotUsername, QStringLiteral("admin"));

    // 管理员主界面:5 个管理页签
    AdminMainWindow mainWin;
    mainWin.show();
    QTabWidget *tabs = find<QTabWidget>(&mainWin, "mainTabs");
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 5);
}

// ---- ③ 用户端:手机号免密登录(未注册自动注册,不重复注册) ----
void GuiFlowTest::userLoginFlow()
{
    QFile::remove(QStringLiteral("gui_test_user.db"));

    Server server;
    QVERIFY(server.start(QStringLiteral("gui_test_user.db"),
                         QStringLiteral("127.0.0.1"), 0));
    AppContext::instance()->setServer(QStringLiteral("127.0.0.1"), server.port());

    UserLoginWindow login;
    login.show();
    QVERIFY(QTest::qWaitForWindowExposed(&login));

    QLineEdit *phoneEdit = find<QLineEdit>(&login, "phoneEdit");
    QPushButton *loginBtn = find<QPushButton>(&login, "loginButton");
    QVERIFY(phoneEdit && loginBtn);

    bool gotSignal = false;
    DataRow gotUser;
    connect(&login, &UserLoginWindow::loginSucceeded, this,
            [&](const DataRow &u) { gotSignal = true; gotUser = u; });

    // 10 位 → 非法,弹提示,不发信号
    phoneEdit->setText(QStringLiteral("1380013800"));
    QTest::mouseClick(loginBtn, Qt::LeftButton);
    QVERIFY(!gotSignal);

    // 新手机号 → 自动注册 + 登录,昵称 = "用户" + 手机号后四位
    const QString phone = QStringLiteral("13800138000");
    phoneEdit->setText(phone);
    QTest::mouseClick(loginBtn, Qt::LeftButton);
    QVERIFY(gotSignal);
    QVERIFY(!gotUser.isEmpty());
    QCOMPARE(gotUser.value(QStringLiteral("phone")).toString(), phone);
    QCOMPARE(gotUser.value(QStringLiteral("nickname")).toString(), QStringLiteral("用户8000"));
    QVERIFY(gotUser.value(QStringLiteral("id")).toLongLong() > 0);

    // 库里该手机号只有一行(只注册过一次)
    DataSource *ds = AppContext::instance()->dataSource();
    QueryResult rows = ds->query(QStringLiteral("users"), {},
                                 QStringLiteral("phone = ?"), QVariantList{phone});
    QCOMPARE(rows.size(), 1);

    // 同一手机号再登录 → 不重复注册,仍成功
    gotSignal = false;
    QTest::mouseClick(loginBtn, Qt::LeftButton);
    QVERIFY(gotSignal);
    rows = ds->query(QStringLiteral("users"), {},
                     QStringLiteral("phone = ?"), QVariantList{phone});
    QCOMPARE(rows.size(), 1);

    // 用户主界面:页面栈 5 页(电站列表 / 电站详情 / 充电 / 订单 / 我的)
    UserMainWindow mainWin;
    mainWin.show();
    QStackedWidget *stack = find<QStackedWidget>(&mainWin, "pageStack");
    QVERIFY(stack);
    QCOMPARE(stack->count(), 5);
}

// ---- ④ 充电业务数据:种子电站/电桩 + 新列存在 ----
void GuiFlowTest::chargingSeedTest()
{
    QFile::remove(QStringLiteral("gui_test_seed.db"));

    Server server;
    QVERIFY(server.start(QStringLiteral("gui_test_seed.db"),
                         QStringLiteral("127.0.0.1"), 0));
    AppContext::instance()->setServer(QStringLiteral("127.0.0.1"), server.port());
    QVERIFY(AppContext::instance()->connectIfNeeded());
    DataSource *ds = AppContext::instance()->dataSource();
    QVERIFY(ds);

    // 种子电站
    const QueryResult stations = ds->query(QStringLiteral("charging_stations"));
    QVERIFY(stations.size() >= 3);
    const DataRow st = stations.first();
    QVERIFY(st.contains(QStringLiteral("price_per_kwh")));
    QVERIFY(st.value(QStringLiteral("price_per_kwh")).toDouble() > 0.0);

    // 种子电桩:每站有桩,累计字段存在且初值为 0
    const QueryResult piles = ds->query(QStringLiteral("charging_piles"));
    QVERIFY(piles.size() >= 6);
    const DataRow p = piles.first();
    QVERIFY(p.contains(QStringLiteral("charge_count")));
    QVERIFY(p.contains(QStringLiteral("charge_duration_min")));
    QCOMPARE(p.value(QStringLiteral("charge_count")).toLongLong(), 0LL);

    // 默认管理员仍在
    const QueryResult admins = ds->query(QStringLiteral("admins"), {},
                                         QStringLiteral("username = 'admin'"));
    QCOMPARE(admins.size(), 1);
}

// ---- ⑤ 完整充电链路:选站 → 选桩 → 开始 → 进度 → 结束结算 ----
void GuiFlowTest::chargeFlow()
{
    QFile::remove(QStringLiteral("gui_test_charge.db"));

    Server server;
    QVERIFY(server.start(QStringLiteral("gui_test_charge.db"),
                         QStringLiteral("127.0.0.1"), 0));
    AppContext::instance()->setServer(QStringLiteral("127.0.0.1"), server.port());
    QVERIFY(AppContext::instance()->connectIfNeeded());
    DataSource *ds = AppContext::instance()->dataSource();
    QVERIFY(ds);

    // 造一个带余额的测试用户并设为当前登录用户
    QHash<QString, QVariant> u;
    u.insert(QStringLiteral("phone"), QStringLiteral("13900000001"));
    u.insert(QStringLiteral("nickname"), QStringLiteral("测试用户"));
    u.insert(QStringLiteral("balance"), 100.0);
    const qlonglong userId = ds->insertRow(QStringLiteral("users"), u);
    QVERIFY(userId > 0);
    const QueryResult userRows = ds->query(QStringLiteral("users"), {},
                                           QStringLiteral("id = ?"), QVariantList{userId});
    QVERIFY(!userRows.isEmpty());
    AppContext::instance()->setCurrentUser(userRows.first());

    UserMainWindow mainWin;
    mainWin.show();
    QVERIFY(QTest::qWaitForWindowExposed(&mainWin));
    QStackedWidget *stack = find<QStackedWidget>(&mainWin, "pageStack");
    QVERIFY(stack);
    QCOMPARE(stack->currentIndex(), 0); // 默认停在电站列表

    // 选中第一个电站 → 进入详情页
    QListWidget *stationList = find<QListWidget>(&mainWin, "stationList");
    QVERIFY(stationList);
    QVERIFY(stationList->count() > 0);
    QTest::mouseClick(stationList->viewport(), Qt::LeftButton, Qt::NoModifier,
                      stationList->visualItemRect(stationList->item(0)).center());
    QCOMPARE(stack->currentIndex(), 1);

    // 选第一个空闲桩 → 进入充电页
    QListWidget *pileList = find<QListWidget>(&mainWin, "pileList");
    QVERIFY(pileList);
    QVERIFY(pileList->count() > 0);
    QTest::mouseClick(pileList->viewport(), Qt::LeftButton, Qt::NoModifier,
                      pileList->visualItemRect(pileList->item(0)).center());
    QCOMPARE(stack->currentIndex(), 2);

    QPushButton *startBtn = find<QPushButton>(&mainWin, "startChargeBtn");
    QPushButton *stopBtn = find<QPushButton>(&mainWin, "stopChargeBtn");
    QLabel *energyLabel = find<QLabel>(&mainWin, "energyLabel");
    QVERIFY(startBtn && stopBtn && energyLabel);
    QVERIFY(startBtn->isEnabled()); // 无未完成订单,可开始

    // 开始充电
    QTest::mouseClick(startBtn, Qt::LeftButton);
    QVERIFY(!startBtn->isEnabled());
    QVERIFY(stopBtn->isEnabled());

    const QueryResult charging = ds->query(QStringLiteral("orders"), {},
        QStringLiteral("user_id = ? AND status = '充电中'"), QVariantList{userId});
    QCOMPARE(charging.size(), 1);
    const DataRow order = charging.first();
    const qlonglong pileId = order.value(QStringLiteral("pile_id")).toLongLong();
    const QueryResult pileBusy = ds->query(QStringLiteral("charging_piles"), {},
        QStringLiteral("id = ?"), QVariantList{pileId});
    QCOMPARE(pileBusy.first().value(QStringLiteral("status")).toString(),
             QStringLiteral("使用中"));

    // 等 ~2.2 秒(模拟 ~2 分钟),电量应从 0 涨起来
    QTest::qWait(2200);
    const QString energyText = energyLabel->text();
    QVERIFY(energyText.contains(QStringLiteral("kWh")));
    QVERIFY(!energyText.contains(QStringLiteral("0.00 kWh")));

    // 结束充电 → 结算(提示框被守卫自动关掉)
    QTest::mouseClick(stopBtn, Qt::LeftButton);
    QTest::qWait(50);

    const QueryResult done = ds->query(QStringLiteral("orders"), {},
        QStringLiteral("id = ?"), QVariantList{order.value(QStringLiteral("id")).toLongLong()});
    QCOMPARE(done.first().value(QStringLiteral("status")).toString(), QStringLiteral("已完成"));
    QVERIFY(done.first().value(QStringLiteral("energy_kwh")).toDouble() > 0.0);
    QVERIFY(done.first().value(QStringLiteral("amount")).toDouble() > 0.0);

    const QueryResult pileAfter = ds->query(QStringLiteral("charging_piles"), {},
        QStringLiteral("id = ?"), QVariantList{pileId});
    QCOMPARE(pileAfter.first().value(QStringLiteral("status")).toString(),
             QStringLiteral("空闲"));
    QCOMPARE(pileAfter.first().value(QStringLiteral("charge_count")).toLongLong(), 1LL);
    QVERIFY(pileAfter.first().value(QStringLiteral("charge_duration_min")).toLongLong() >= 1);

    // 用户余额被扣(100 起步,结算后应小于 100)
    const QueryResult userAfter = ds->query(QStringLiteral("users"), {},
        QStringLiteral("id = ?"), QVariantList{userId});
    QVERIFY(userAfter.first().value(QStringLiteral("balance")).toDouble() < 100.0);
}

// ---- ⑥ 未完成订单拦截:有"充电中"订单时,进充电页被拦下并跳到订单页 ----
void GuiFlowTest::unfinishedOrderCheck()
{
    QFile::remove(QStringLiteral("gui_test_unfinished.db"));

    Server server;
    QVERIFY(server.start(QStringLiteral("gui_test_unfinished.db"),
                         QStringLiteral("127.0.0.1"), 0));
    AppContext::instance()->setServer(QStringLiteral("127.0.0.1"), server.port());
    QVERIFY(AppContext::instance()->connectIfNeeded());
    DataSource *ds = AppContext::instance()->dataSource();
    QVERIFY(ds);

    // 造用户 + 一笔直接插入的"充电中"订单
    QHash<QString, QVariant> u;
    u.insert(QStringLiteral("phone"), QStringLiteral("13900000002"));
    u.insert(QStringLiteral("nickname"), QStringLiteral("待结算用户"));
    const qlonglong userId = ds->insertRow(QStringLiteral("users"), u);
    QVERIFY(userId > 0);
    const QueryResult userRows = ds->query(QStringLiteral("users"), {},
                                           QStringLiteral("id = ?"), QVariantList{userId});
    QVERIFY(!userRows.isEmpty());
    AppContext::instance()->setCurrentUser(userRows.first());

    const QueryResult pileRows = ds->query(QStringLiteral("charging_piles"));
    QVERIFY(!pileRows.isEmpty());
    QHash<QString, QVariant> order;
    order.insert(QStringLiteral("user_id"), userId);
    order.insert(QStringLiteral("pile_id"), pileRows.first().value(QStringLiteral("id")).toLongLong());
    order.insert(QStringLiteral("start_time"),
                 QDateTime::currentDateTime().addSecs(-120).toString(Qt::ISODate));
    const qlonglong orderId = ds->insertRow(QStringLiteral("orders"), order);
    QVERIFY(orderId > 0);

    UserMainWindow mainWin;
    mainWin.show();
    QVERIFY(QTest::qWaitForWindowExposed(&mainWin));
    QStackedWidget *stack = find<QStackedWidget>(&mainWin, "pageStack");
    QVERIFY(stack);

    // 点"充电"导航 → 有未完成订单 → 弹提示(守卫关掉) → 自动跳到订单页
    QPushButton *navCharge = find<QPushButton>(&mainWin, "navChargeBtn");
    QVERIFY(navCharge);
    QTest::mouseClick(navCharge, Qt::LeftButton);
    QTest::qWait(50);
    QCOMPARE(stack->currentIndex(), 3); // 订单页

    // 订单列表里应能看到这条"充电中"订单,可结算
    QListWidget *orderList = find<QListWidget>(&mainWin, "orderList");
    QPushButton *settleBtn = find<QPushButton>(&mainWin, "settleBtn");
    QVERIFY(orderList && settleBtn);
    QCOMPARE(orderList->count(), 1);
    orderList->setCurrentRow(0);
    QVERIFY(settleBtn->isEnabled());

    // 用户余额为 0(自动注册默认值),结算被服务端余额守卫拦下 → 订单保持"充电中"
    QTest::mouseClick(settleBtn, Qt::LeftButton);
    QTest::qWait(50);
    QueryResult afterFirst = ds->query(QStringLiteral("orders"), {},
        QStringLiteral("id = ?"), QVariantList{orderId});
    QCOMPARE(afterFirst.first().value(QStringLiteral("status")).toString(),
             QStringLiteral("充电中"));

    // 充值后再结算 → 成功,订单完成
    QVERIFY(ds->rechargeBalance(userId, 1000.0));
    // 失败后列表已刷新,重新选中该行才能点亮结算按钮
    orderList->setCurrentRow(0);
    QVERIFY(settleBtn->isEnabled());
    QTest::mouseClick(settleBtn, Qt::LeftButton);
    QTest::qWait(50);
    const QueryResult done = ds->query(QStringLiteral("orders"), {},
        QStringLiteral("id = ?"), QVariantList{orderId});
    QCOMPARE(done.first().value(QStringLiteral("status")).toString(), QStringLiteral("已完成"));
}

QTEST_MAIN(GuiFlowTest)
#include "gui_flow_test.moc"
