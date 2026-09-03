#ifndef MAPSERVICE_H
#define MAPSERVICE_H

#include <QObject>
#include <QString>
#include <QVariant>

class QNetworkAccessManager;

// 腾讯位置服务的轻量封装。地址解析与地点候选集中放在这里，
// 页面不直接拼接 Web API 请求；后续管理员录入地址时也可复用。
class MapService : public QObject
{
    Q_OBJECT
public:
    explicit MapService(QObject *parent = nullptr);

    void setApiKey(const QString &apiKey);
    QString apiKey() const;
    bool hasApiKey() const;

    // 异步解析地址；region 可传“北京”等城市名，也可以留空。
    void geocode(const QString &address, const QString &region = QString());
    // 异步获取地点候选；每个结果包含 title/address/lat/lng。
    void suggest(const QString &keyword, const QString &region = QString());
    // 获取驾车或步行路线；path 中每项包含 lat/lng。
    void planRoute(double fromLat, double fromLng, double toLat, double toLng,
                   const QString &mode);

signals:
    void geocodeSucceeded(const QString &address, double latitude, double longitude);
    void suggestionsSucceeded(const QVariantList &suggestions);
    void suggestionsFailed(const QString &message);
    void routeSucceeded(const QString &mode, const QVariantList &path,
                        int distanceMeters, int durationMinutes);
    void routeFailed(const QString &message);
    void requestFailed(const QString &message);

private:
    QString loadConfiguredApiKey() const;

    QNetworkAccessManager *m_network = nullptr;
    QString m_apiKey;
    quint64 m_suggestionSerial = 0;
    quint64 m_routeSerial = 0;
};

#endif // MAPSERVICE_H
