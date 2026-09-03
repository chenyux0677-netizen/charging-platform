#ifndef STATIONDETAILPAGE_H
#define STATIONDETAILPAGE_H

#include "core/DataSource.h"

#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;

// 用户端 · 充电站详情页:展示站名 / 地址 + 站内充电桩列表。
// 点击"空闲"的桩 → 进入充电页;占用中的桩 → 提示不可用。
class StationDetailPage : public QWidget
{
    Q_OBJECT
public:
    explicit StationDetailPage(QWidget *parent = nullptr);

    // 切换到某电站:刷新站名/地址与桩列表
    void setStation(const DataRow &station);

signals:
    void backRequested();
    void pileChosen(const DataRow &pile);
    void navigationRequested(const DataRow &station);

private:
    void refreshPiles();
    void onPileClicked();

    QPushButton *m_backButton = nullptr;
    QPushButton *m_navigationButton = nullptr;
    QLabel *m_stationNameLabel = nullptr;
    QLabel *m_stationAddrLabel = nullptr;
    QListWidget *m_pileList = nullptr;
    DataRow m_station;
};

#endif // STATIONDETAILPAGE_H
