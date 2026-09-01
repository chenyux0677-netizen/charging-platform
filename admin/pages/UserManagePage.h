#ifndef USERMANAGEPAGE_H
#define USERMANAGEPAGE_H

#include <QWidget>

// 管理员端 · 用户管理页(待填充:用户列表 / 封号 / 解封)
class UserManagePage : public QWidget
{
    Q_OBJECT
public:
    explicit UserManagePage(QWidget *parent = nullptr);
};

#endif // USERMANAGEPAGE_H
