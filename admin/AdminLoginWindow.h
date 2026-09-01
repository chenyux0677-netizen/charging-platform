#ifndef ADMINLOGINWINDOW_H
#define ADMINLOGINWINDOW_H

#include <QWidget>

class QLineEdit;
class QPushButton;

// 管理员端登录:账号 + 密码。
// 默认账号 admin / 123456,由服务器启动时写入种子数据。
// 登录窗口只负责校验,成功发信号,由调用方切换窗口。
class AdminLoginWindow : public QWidget
{
    Q_OBJECT
public:
    explicit AdminLoginWindow(QWidget *parent = nullptr);

signals:
    void loginSucceeded(const QString &username);

private slots:
    void onLoginClicked();

private:
    QLineEdit *m_userEdit = nullptr;
    QLineEdit *m_passEdit = nullptr;
    QPushButton *m_loginButton = nullptr;
};

#endif // ADMINLOGINWINDOW_H
