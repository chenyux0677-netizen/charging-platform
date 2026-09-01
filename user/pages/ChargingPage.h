#ifndef CHARGINGPAGE_H
#define CHARGINGPAGE_H

#include <QWidget>

// 用户端 · 充电中页(待填充:当前充电进度 / 电量 / 费用)
class ChargingPage : public QWidget
{
    Q_OBJECT
public:
    explicit ChargingPage(QWidget *parent = nullptr);
};

#endif // CHARGINGPAGE_H
