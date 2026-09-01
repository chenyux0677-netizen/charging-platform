#ifndef STATIONDETAILPAGE_H
#define STATIONDETAILPAGE_H

#include <QWidget>

// 用户端 · 充电站详情页(待填充:站内充电桩列表 → 点桩开始充电)
class StationDetailPage : public QWidget
{
    Q_OBJECT
public:
    explicit StationDetailPage(QWidget *parent = nullptr);
};

#endif // STATIONDETAILPAGE_H
