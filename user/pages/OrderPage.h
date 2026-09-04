#ifndef ORDERPAGE_H
#define ORDERPAGE_H

#include <QWidget>

class QListWidget;
class QPushButton;

// 用户端 · 我的订单页:当前用户的充电订单列表。
// “充电中”订单可结束并支付，“待支付”订单可在充值后继续支付。
class OrderPage : public QWidget
{
    Q_OBJECT
public:
    explicit OrderPage(QWidget *parent = nullptr);

    void refresh();

private:
    void onSettleClicked();
    void onCurrentRowChanged();

    QListWidget *m_orderList = nullptr;
    QPushButton *m_settleBtn = nullptr;
};

#endif // ORDERPAGE_H
