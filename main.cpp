#include "common/AppContext.h"
#include "common/RoleSelectWindow.h"

#include "admin/AdminLoginWindow.h"
#include "admin/AdminMainWindow.h"
#include "server/Server.h"
#include "user/UserLoginWindow.h"
#include "user/UserMainWindow.h"

#include <QApplication>
#include <QFile>
#include <QMessageBox>

namespace {
// 管理员端内嵌服务器的固定端口(用户端在角色选择页填同样的端口)
constexpr quint16 kDefaultPort = 9527;

// 把若干 qrc 内的样式文件拼接成一个样式表(后者覆盖前者)。
// common.qss 是两端共用基底,user/admin.qss 是其上的模式覆盖。
void applyStyle(const QStringList &qrcPaths)
{
    QString css;
    for (const QString &path : qrcPaths) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly))
            css += QString::fromUtf8(file.readAll()) + QLatin1Char('\n');
    }
    qApp->setStyleSheet(css);
}
} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 1) 启动页:先套共用基底样式,再显示角色选择(否则首屏无样式)
    applyStyle({QStringLiteral(":/styles/common.qss")});
    RoleSelectWindow roleSelect;
    roleSelect.show();

    // 2a) 用户模式:连接服务器(跨设备演示时,填管理员那台机器的局域网 IP),进用户登录
    QObject::connect(&roleSelect, &RoleSelectWindow::userModeSelected, &app,
                     [&](const QString &host, quint16 port) {
        AppContext::instance()->setServer(host, port);
        applyStyle({QStringLiteral(":/styles/common.qss"),
                    QStringLiteral(":/styles/user.qss")});

        auto *login = new UserLoginWindow;
        login->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(login, &UserLoginWindow::loginSucceeded, &app,
                         [&, login](const DataRow &user) {
            AppContext::instance()->setCurrentUser(user);

            auto *mainWin = new UserMainWindow;
            mainWin->setAttribute(Qt::WA_DeleteOnClose);
            mainWin->show();
            login->close(); // DeleteOnClose → 登录窗口自动销毁

            // 退出登录 → 回到角色选择页,再次选择
            QObject::connect(mainWin, &UserMainWindow::logoutRequested, &app,
                             [&, mainWin] {
                roleSelect.show();
                mainWin->close(); // DeleteOnClose → 主窗口自动销毁
            });
        });
        login->show();
        roleSelect.hide();
    });

    // 2b) 管理员模式:本机启动内嵌服务器(数据库中间层),再进管理员登录
    QObject::connect(&roleSelect, &RoleSelectWindow::adminModeSelected, &app,
                     [&] {
        auto *server = new Server(&app);
        if (!server->start(QStringLiteral("app.db"),
                           QStringLiteral("0.0.0.0"), kDefaultPort)) {
            QMessageBox::critical(&roleSelect, QStringLiteral("启动失败"),
                                  QStringLiteral("服务器启动失败,请检查端口 %1 是否被占用。")
                                      .arg(kDefaultPort));
            return;
        }
        AppContext::instance()->setServer(QStringLiteral("127.0.0.1"), kDefaultPort);
        applyStyle({QStringLiteral(":/styles/common.qss"),
                    QStringLiteral(":/styles/admin.qss")});

        auto *login = new AdminLoginWindow;
        login->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(login, &AdminLoginWindow::loginSucceeded, &app,
                         [&, login](const QString &username) {
            AppContext::instance()->setCurrentAdmin(username);

            auto *mainWin = new AdminMainWindow;
            mainWin->setAttribute(Qt::WA_DeleteOnClose);
            mainWin->show();
            login->close();
        });
        login->show();
        roleSelect.hide();
    });

    return app.exec();
}
