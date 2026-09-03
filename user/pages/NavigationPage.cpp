#include "NavigationPage.h"

#include "user/services/MapService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEngineView>

namespace {
QString jsonString(const QString &value)
{
    const QJsonArray array{value};
    const QByteArray json = QJsonDocument(array).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(json.mid(1, json.size() - 2));
}

QString routePathScript(const QVariantList &path)
{
    QStringList points;
    points.reserve(path.size());
    for (const QVariant &value : path) {
        const QVariantMap point = value.toMap();
        points.append(QStringLiteral("new TMap.LatLng(%1,%2)")
                          .arg(point.value(QStringLiteral("lat")).toDouble(), 0, 'f', 8)
                          .arg(point.value(QStringLiteral("lng")).toDouble(), 0, 'f', 8));
    }
    if (points.isEmpty())
        return {};
    return QStringLiteral(R"JS(
const routePath = [%1];
window.routePolyline = new TMap.MultiPolyline({
  map: map,
  styles: {route: new TMap.PolylineStyle({
    color:'#3777ff', width:8, borderWidth:2, borderColor:'#ffffff', lineCap:'round'
  })},
  geometries: [{id:'route', styleId:'route', paths:routePath}]
});
routePath.forEach(point => bounds.extend(point));
)JS").arg(points.join(QLatin1Char(',')));
}
}

NavigationPage::NavigationPage(QWidget *parent)
    : QWidget(parent)
{
    auto *backButton = new QPushButton(QStringLiteral("← 返回电站详情"), this);
    backButton->setObjectName(QStringLiteral("navigationBackButton"));

    m_titleLabel = new QLabel(QStringLiteral("地图导航"), this);
    m_titleLabel->setObjectName(QStringLiteral("pageTitleLabel"));
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setWordWrap(true);

    m_statusLabel = new QLabel(QStringLiteral("请选择目标电站"), this);
    m_statusLabel->setObjectName(QStringLiteral("mapStatusLabel"));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);

    m_drivingButton = new QPushButton(QStringLiteral("驾车"), this);
    m_drivingButton->setObjectName(QStringLiteral("drivingRouteButton"));
    m_drivingButton->setCheckable(true);
    m_walkingButton = new QPushButton(QStringLiteral("步行"), this);
    m_walkingButton->setObjectName(QStringLiteral("walkingRouteButton"));
    m_walkingButton->setCheckable(true);
    auto *modeGroup = new QButtonGroup(this);
    modeGroup->setExclusive(true);
    modeGroup->addButton(m_drivingButton);
    modeGroup->addButton(m_walkingButton);
    m_drivingButton->setChecked(true);

    auto *modeLayout = new QHBoxLayout;
    modeLayout->addStretch();
    modeLayout->addWidget(m_drivingButton);
    modeLayout->addWidget(m_walkingButton);
    modeLayout->addStretch();

    auto *mapHost = new QWidget(this);
    mapHost->setObjectName(QStringLiteral("mapHost"));
    m_mapLayout = new QVBoxLayout(mapHost);
    m_mapLayout->setContentsMargins(0, 0, 0, 0);
    m_mapLayout->addWidget(m_statusLabel);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(backButton);
    layout->addWidget(m_titleLabel);
    layout->addLayout(modeLayout);
    layout->addWidget(mapHost, 1);

    connect(backButton, &QPushButton::clicked, this, &NavigationPage::backRequested);
    connect(m_drivingButton, &QPushButton::clicked, this,
            [this] { requestRoute(QStringLiteral("driving")); });
    connect(m_walkingButton, &QPushButton::clicked, this,
            [this] { requestRoute(QStringLiteral("walking")); });

    m_mapService = new MapService(this);
    connect(m_mapService, &MapService::routeSucceeded, this,
            [this](const QString &mode, const QVariantList &path,
                   int distanceMeters, int durationMinutes) {
        const QString modeName = mode == QStringLiteral("walking")
            ? QStringLiteral("步行") : QStringLiteral("驾车");
        m_statusLabel->setText(QStringLiteral("%1路线：%2 km，预计 %3 分钟")
                                   .arg(modeName)
                                   .arg(distanceMeters / 1000.0, 0, 'f', 1)
                                   .arg(qMax(1, durationMinutes)));
        m_webView->setHtml(
            buildMapHtml(m_mapService->apiKey(), m_originName,
                         m_originLat, m_originLng, m_destinationName,
                         m_destinationLat, m_destinationLng, path),
            QUrl(QStringLiteral("https://map.qq.com/")));
    });
    connect(m_mapService, &MapService::routeFailed, this,
            [this](const QString &message) {
        m_statusLabel->setText(message);
        m_webView->setHtml(
            buildMapHtml(m_mapService->apiKey(), m_originName,
                         m_originLat, m_originLng, m_destinationName,
                         m_destinationLat, m_destinationLng),
            QUrl(QStringLiteral("https://map.qq.com/")));
    });
}

void NavigationPage::showLocations(const QString &originName,
                                   double originLat, double originLng,
                                   const DataRow &station)
{
    m_originName = originName;
    m_originLat = originLat;
    m_originLng = originLng;
    m_destinationName = station.value(QStringLiteral("name")).toString();
    m_destinationLat = station.value(QStringLiteral("lat")).toDouble();
    m_destinationLng = station.value(QStringLiteral("lng")).toDouble();
    m_titleLabel->setText(QStringLiteral("路线规划至 %1").arg(m_destinationName));

    if (!m_mapService->hasApiKey()) {
        m_statusLabel->setText(QStringLiteral("未配置腾讯地图 Key"));
        if (m_webView)
            m_webView->hide();
        return;
    }

    ensureWebView();
    m_statusLabel->show();
    m_webView->show();
    m_drivingButton->setChecked(true);
    requestRoute(QStringLiteral("driving"));
}

void NavigationPage::ensureWebView()
{
    if (m_webView)
        return;
    m_webView = new QWebEngineView(this);
    m_webView->setObjectName(QStringLiteral("navigationWebView"));
    m_mapLayout->addWidget(m_webView, 1);
    connect(m_webView, &QWebEngineView::loadFinished, this, [this](bool ok) {
        if (!ok)
            m_statusLabel->setText(QStringLiteral("地图页面加载失败"));
    });
}

void NavigationPage::requestRoute(const QString &mode)
{
    if (!m_webView || !m_mapService->hasApiKey())
        return;
    const bool walking = mode == QStringLiteral("walking");
    m_walkingButton->setChecked(walking);
    m_drivingButton->setChecked(!walking);
    m_statusLabel->setText(walking ? QStringLiteral("正在规划步行路线……")
                                   : QStringLiteral("正在规划驾车路线……"));
    m_mapService->planRoute(m_originLat, m_originLng,
                            m_destinationLat, m_destinationLng, mode);
}

QString NavigationPage::buildMapHtml(const QString &apiKey,
                                    const QString &originName,
                                    double originLat, double originLng,
                                    const QString &destinationName,
                                    double destinationLat, double destinationLng,
                                    const QVariantList &path) const
{
    return QStringLiteral(R"HTML(
<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<style>html,body,#map{width:100%;height:100%;margin:0;overflow:hidden}</style>
<script src="https://map.qq.com/api/gljs?v=1.exp&key=%1"></script>
</head><body><div id="map"></div><script>
const origin = new TMap.LatLng(%2, %3);
const destination = new TMap.LatLng(%4, %5);
const map = new TMap.Map('map', {
  center: new TMap.LatLng((%2 + %4) / 2, (%3 + %5) / 2),
  zoom: 13,
  pitch: 0
});
new TMap.MultiMarker({
  map: map,
  geometries: [
    {id:'origin', position:origin, title:%6},
    {id:'destination', position:destination, title:%7}
  ]
});
const bounds = new TMap.LatLngBounds();
bounds.extend(origin);
bounds.extend(destination);
%8
map.fitBounds(bounds, {padding:60});
</script></body></html>)HTML")
        .arg(apiKey.toHtmlEscaped())
        .arg(originLat, 0, 'f', 8)
        .arg(originLng, 0, 'f', 8)
        .arg(destinationLat, 0, 'f', 8)
        .arg(destinationLng, 0, 'f', 8)
        .arg(jsonString(originName))
        .arg(jsonString(destinationName))
        .arg(routePathScript(path));
}
