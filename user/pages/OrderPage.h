#ifndef ORDERPAGE_H
#define ORDERPAGE_H

#include <QWidget>

class QListWidget;
class QPushButton;

// 用户端 · 我的订单页:当前用户的充电订单列表。
// 选中"充电中"的订单时可结算(结算逻辑与充电页共用 ChargeService)。
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
