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
#include <QDialog>
#include <QLineEdit>
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

    // 用户主界面:页面栈 4 页(电站 / 充电 / 订单 / 我的)
    UserMainWindow mainWin;
    mainWin.show();
    QStackedWidget *stack = find<QStackedWidget>(&mainWin, "pageStack");
    QVERIFY(stack);
    QCOMPARE(stack->count(), 4);
}

QTEST_MAIN(GuiFlowTest)
#include "gui_flow_test.moc"
