#ifndef ORDERPAGE_H
#define ORDERPAGE_H

#include <QWidget>

// 用户端 · 我的订单页(待填充:订单列表 / 状态)
class OrderPage : public QWidget
{
    Q_OBJECT
public:
    explicit OrderPage(QWidget *parent = nullptr);
};

#endif // ORDERPAGE_H
