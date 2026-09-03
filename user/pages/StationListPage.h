#ifndef STATIONLISTPAGE_H
#define STATIONLISTPAGE_H

#include "core/DataSource.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QTimer;
class MapService;

// 用户端 · 附近充电站列表页。
// 无真实定位,用"模拟定位"(城市区域下拉)代替 GPS,
// 按球面距离(haversine)升序展示各充电站下属电桩的价格范围 / 总数 / 空闲数 / 距离。
class StationListPage : public QWidget
{
    Q_OBJECT
public:
    explicit StationListPage(QWidget *parent = nullptr);

    // 重新拉取电站并按距当前模拟定位的距离升序刷新
    void refresh();
    double currentLatitude() const { return m_currentLat; }
    double currentLongitude() const { return m_currentLng; }
    QString currentLocationName() const;

signals:
    // 点击某电站 → 带上该站整行数据
    void stationClicked(const DataRow &station);

private:
    void onItemClicked();
    void onRegionChanged();
    void requestSuggestions();
    void useSelectedSuggestion();

    QComboBox *m_regionCombo = nullptr;
    QLineEdit *m_addressEdit = nullptr;
    QLabel *m_locationStatusLabel = nullptr;
    QListWidget *m_suggestionList = nullptr;
    QListWidget *m_stationList = nullptr;
    MapService *m_mapService = nullptr;
    QTimer *m_suggestionTimer = nullptr;
    double m_currentLat = 0.0;
    double m_currentLng = 0.0;
};

#endif // STATIONLISTPAGE_H
