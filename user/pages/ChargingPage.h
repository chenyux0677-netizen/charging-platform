#ifndef CHARGINGPAGE_H
#define CHARGINGPAGE_H

#include "core/DataSource.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QTimer;

// 用户端 · 充电页:展示当前充电进度(模拟 1 秒 = 1 分钟)、电量、金额。
// 结算一律交给 ChargeService(按订单开始时间 → 当前时间折算),重启也能正确结算。
// 进入页面时先检查"未完成的充电订单":有则提示先去订单页结算。
class ChargingPage : public QWidget
{
    Q_OBJECT
public:
    explicit ChargingPage(QWidget *parent = nullptr);

    // 选中某个空闲充电桩后调用,进入"待开始"状态
    void setPile(const DataRow &pile, const DataRow &station);

    // 每次切入本页时调用:检查未完成订单,决定能否开始充电
    void onPageEntered();

signals:
    void goToOrders();

private:
    void startCharging();
    void stopCharging();
    void onTick();
    void reportChargingProgress();
    void resetPage();
    bool findUnfinishedOrder() const;

    QLabel *m_stationLabel = nullptr;
    QLabel *m_pileLabel = nullptr;
    QLabel *m_priceLabel = nullptr;
    QLabel *m_energyLabel = nullptr;
    QLabel *m_amountLabel = nullptr;
    QLabel *m_timeLabel = nullptr;
    QPushButton *m_startChargeBtn = nullptr;
    QPushButton *m_stopChargeBtn = nullptr;
    QTimer *m_timer = nullptr;

    DataRow m_pile;
    double m_power = 0.0;
    double m_price = 0.0;
    qlonglong m_orderId = 0;
    int m_minutes = 0;
    bool m_charging = false;
    bool m_progressBusy = false;
};

#endif // CHARGINGPAGE_H
