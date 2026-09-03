#ifndef NAVIGATIONPAGE_H
#define NAVIGATIONPAGE_H

#include "core/DataSource.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;
class QWebEngineView;
class MapService;

// 用户端路线规划页：显示起终点，并支持驾车/步行路线切换。
class NavigationPage : public QWidget
{
    Q_OBJECT
public:
    explicit NavigationPage(QWidget *parent = nullptr);

    void showLocations(const QString &originName, double originLat, double originLng,
                       const DataRow &station);

signals:
    void backRequested();

private:
    void ensureWebView();
    void requestRoute(const QString &mode);
    QString buildMapHtml(const QString &apiKey, const QString &originName,
                         double originLat, double originLng,
                         const QString &destinationName,
                         double destinationLat, double destinationLng,
                         const QVariantList &path = {}) const;

    QLabel *m_titleLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_drivingButton = nullptr;
    QPushButton *m_walkingButton = nullptr;
    QVBoxLayout *m_mapLayout = nullptr;
    QWebEngineView *m_webView = nullptr;
    MapService *m_mapService = nullptr;
    QString m_originName;
    QString m_destinationName;
    double m_originLat = 0.0;
    double m_originLng = 0.0;
    double m_destinationLat = 0.0;
    double m_destinationLng = 0.0;
};

#endif // NAVIGATIONPAGE_H
