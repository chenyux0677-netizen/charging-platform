#ifndef STATIONLISTPAGE_H
#define STATIONLISTPAGE_H

#include "core/DataSource.h"

#include <QWidget>

class QComboBox;
class QListWidget;

// 用户端 · 附近充电站列表页。
// 无真实定位,用"模拟定位"(城市区域下拉)代替 GPS,
// 按球面距离(haversine)升序展示各充电站的价格 / 电桩总数 / 空闲数 / 距离。
class StationListPage : public QWidget
{
    Q_OBJECT
public:
    explicit StationListPage(QWidget *parent = nullptr);

    // 重新拉取电站并按距当前模拟定位的距离升序刷新
    void refresh();

signals:
    // 点击某电站 → 带上该站整行数据
    void stationClicked(const DataRow &station);

private:
    void onItemClicked();

    QComboBox *m_regionCombo = nullptr;
    QListWidget *m_stationList = nullptr;
};

#endif // STATIONLISTPAGE_H
