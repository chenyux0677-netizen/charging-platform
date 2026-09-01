#ifndef PROFILEPAGE_H
#define PROFILEPAGE_H

#include <QWidget>

// 用户端 · 我的页(待填充:头像昵称 / 余额 / 退出登录)
class ProfilePage : public QWidget
{
    Q_OBJECT
public:
    explicit ProfilePage(QWidget *parent = nullptr);
};

#endif // PROFILEPAGE_H
