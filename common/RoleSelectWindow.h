#ifndef ROLESELECTWINDOW_H
#define ROLESELECTWINDOW_H

#include <QWidget>

class QLineEdit;
class QPushButton;

// 启动页:先选角色(用户端 / 管理员端),再进对应登录。
// 用户端需要填服务器地址 —— 跨设备演示时,填管理员那台机器的局域网 IP;
// 同设备演示时保持默认 127.0.0.1:9527 即可。
class RoleSelectWindow : public QWidget
{
    Q_OBJECT
public:
    explicit RoleSelectWindow(QWidget *parent = nullptr);

signals:
    // 选了用户端,带上服务器地址
    void userModeSelected(const QString &host, quint16 port);
    // 选了管理员端(管理员模式会本机起内嵌服务器)
    void adminModeSelected();

private slots:
    void onUserButtonClicked();
    void onAdminButtonClicked();

private:
    QLineEdit *m_hostEdit = nullptr;
    QLineEdit *m_portEdit = nullptr;
    QPushButton *m_userButton = nullptr;
    QPushButton *m_adminButton = nullptr;
};

#endif // ROLESELECTWINDOW_H
