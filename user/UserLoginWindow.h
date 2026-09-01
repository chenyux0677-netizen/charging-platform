#ifndef USERLOGINWINDOW_H
#define USERLOGINWINDOW_H

#include "core/DataSource.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

// 用户端登录:手机号即账号,免密。
//   手机号不存在 → 自动注册后登录;
//   已存在 → 直接登录;
//   状态为"停用"(管理员封号) → 拒绝登录。
// 登录窗口只负责校验与取数,登录成功只发信号,
// 由调用方(main.cpp 或测试)负责切换窗口。
class UserLoginWindow : public QWidget
{
    Q_OBJECT
public:
    explicit UserLoginWindow(QWidget *parent = nullptr);

signals:
    // 登录成功,携带用户整行数据(含 id / nickname / balance / status)
    void loginSucceeded(const DataRow &user);

private slots:
    void onLoginClicked();

private:
    bool tryConnect();
    DataRow findOrCreateUser(const QString &phone);

    QLineEdit *m_phoneEdit = nullptr;
    QPushButton *m_loginButton = nullptr;
};

#endif // USERLOGINWINDOW_H
