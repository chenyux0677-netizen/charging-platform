#ifndef PROFILEPAGE_H
#define PROFILEPAGE_H

#include <QWidget>

class QLabel;
class QPushButton;

// 用户端 · 我的页:头像 / 昵称 / 余额 / 充值 / 改昵称 / 退出登录。
class ProfilePage : public QWidget
{
    Q_OBJECT
public:
    explicit ProfilePage(QWidget *parent = nullptr);

    void refresh();

signals:
    void logoutRequested();

private:
    void onRecharge();
    void onChangeNickname();
    void onAvatarClicked();
    void reloadUser();

    QPushButton *m_avatarBtn = nullptr;
    QLabel *m_nicknameLabel = nullptr;
    QLabel *m_balanceLabel = nullptr;
    QPushButton *m_rechargeBtn = nullptr;
    QPushButton *m_changeNicknameBtn = nullptr;
    QPushButton *m_logoutBtn = nullptr;
};

#endif // PROFILEPAGE_H
