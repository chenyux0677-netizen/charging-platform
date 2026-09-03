#include "MapService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>
#include <QVector>

namespace {
constexpr int kRequestTimeoutMs = 10000;

QString keyFromFile(const QString &path)
{
    if (!QFileInfo::exists(path))
        return {};
    QSettings settings(path, QSettings::IniFormat);
    return settings.value(QStringLiteral("tencent/api_key")).toString().trimmed();
}
}

MapService::MapService(QObject *parent)
    : QObject(parent),
      m_network(new QNetworkAccessManager(this)),
      m_apiKey(loadConfiguredApiKey())
{
}

void MapService::setApiKey(const QString &apiKey)
{
    m_apiKey = apiKey.trimmed();
}

QString MapService::apiKey() const
{
    return m_apiKey;
}

bool MapService::hasApiKey() const
{
    return !m_apiKey.isEmpty();
}

void MapService::geocode(const QString &address, const QString &region)
{
    const QString cleanAddress = address.trimmed();
    if (cleanAddress.isEmpty()) {
        emit requestFailed(QStringLiteral("请输入需要定位的地址"));
        return;
    }
    if (!hasApiKey()) {
        emit requestFailed(QStringLiteral(
            "未配置腾讯地图 Key，请设置 TENCENT_MAP_KEY 或在 map.ini 中填写"));
        return;
    }

    QUrl url(QStringLiteral("https://apis.map.qq.com/ws/geocoder/v1/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("address"), cleanAddress);
    if (!region.trimmed().isEmpty())
        query.addQueryItem(QStringLiteral("region"), region.trimmed());
    query.addQueryItem(QStringLiteral("key"), m_apiKey);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setTransferTimeout(kRequestTimeoutMs);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("charging-platform/0.1"));

    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, cleanAddress] {
        const QByteArray payload = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        const QString networkMessage = reply->errorString();
        reply->deleteLater();

        if (networkError != QNetworkReply::NoError) {
            emit requestFailed(QStringLiteral("腾讯地图请求失败：%1").arg(networkMessage));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            emit requestFailed(QStringLiteral("腾讯地图返回的数据格式无效"));
            return;
        }

        const QJsonObject root = document.object();
        const int status = root.value(QStringLiteral("status")).toInt(-1);
        if (status != 0) {
            const QString message = root.value(QStringLiteral("message")).toString();
            emit requestFailed(QStringLiteral("地址解析失败（%1）：%2")
                                   .arg(status)
                                   .arg(message.isEmpty() ? QStringLiteral("未知错误")
                                                          : message));
            return;
        }

        const QJsonObject result = root.value(QStringLiteral("result")).toObject();
        const QJsonObject location = result.value(QStringLiteral("location")).toObject();
        const double latitude = location.value(QStringLiteral("lat")).toDouble(1000.0);
        const double longitude = location.value(QStringLiteral("lng")).toDouble(1000.0);
        if (latitude < -90.0 || latitude > 90.0
            || longitude < -180.0 || longitude > 180.0) {
            emit requestFailed(QStringLiteral("腾讯地图未返回有效坐标"));
            return;
        }

        const QString resolvedAddress =
            result.value(QStringLiteral("title")).toString(cleanAddress);
        emit geocodeSucceeded(resolvedAddress, latitude, longitude);
    });
}

void MapService::suggest(const QString &keyword, const QString &region)
{
    const quint64 serial = ++m_suggestionSerial;
    const QString cleanKeyword = keyword.trimmed();
    if (cleanKeyword.size() < 2) {
        emit suggestionsSucceeded({});
        return;
    }
    if (!hasApiKey()) {
        emit suggestionsFailed(QStringLiteral(
            "未配置腾讯地图 Key，请设置 TENCENT_MAP_KEY 或在 map.ini 中填写"));
        return;
    }

    QUrl url(QStringLiteral("https://apis.map.qq.com/ws/place/v1/suggestion"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("keyword"), cleanKeyword);
    if (!region.trimmed().isEmpty()) {
        query.addQueryItem(QStringLiteral("region"), region.trimmed());
        query.addQueryItem(QStringLiteral("region_fix"), QStringLiteral("1"));
    }
    query.addQueryItem(QStringLiteral("page_size"), QStringLiteral("8"));
    query.addQueryItem(QStringLiteral("key"), m_apiKey);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setTransferTimeout(kRequestTimeoutMs);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("charging-platform/0.1"));

    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, serial] {
        const QByteArray payload = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        const QString networkMessage = reply->errorString();
        reply->deleteLater();

        // 用户可能已继续输入，只处理最后一次请求的结果。
        if (serial != m_suggestionSerial)
            return;
        if (networkError != QNetworkReply::NoError) {
            emit suggestionsFailed(
                QStringLiteral("地点搜索请求失败：%1").arg(networkMessage));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            emit suggestionsFailed(QStringLiteral("腾讯地图返回的数据格式无效"));
            return;
        }

        const QJsonObject root = document.object();
        const int status = root.value(QStringLiteral("status")).toInt(-1);
        if (status != 0) {
            const QString message = root.value(QStringLiteral("message")).toString();
            emit suggestionsFailed(QStringLiteral("地点搜索失败（%1）：%2")
                                       .arg(status)
                                       .arg(message.isEmpty() ? QStringLiteral("未知错误")
                                                              : message));
            return;
        }

        QVariantList suggestions;
        const QJsonArray data = root.value(QStringLiteral("data")).toArray();
        for (const QJsonValue &value : data) {
            const QJsonObject item = value.toObject();
            const QJsonObject location = item.value(QStringLiteral("location")).toObject();
            const double latitude = location.value(QStringLiteral("lat")).toDouble(1000.0);
            const double longitude = location.value(QStringLiteral("lng")).toDouble(1000.0);
            if (latitude < -90.0 || latitude > 90.0
                || longitude < -180.0 || longitude > 180.0)
                continue;
            QVariantMap suggestion;
            suggestion.insert(QStringLiteral("title"),
                              item.value(QStringLiteral("title")).toString());
            suggestion.insert(QStringLiteral("address"),
                              item.value(QStringLiteral("address")).toString());
            suggestion.insert(QStringLiteral("lat"), latitude);
            suggestion.insert(QStringLiteral("lng"), longitude);
            suggestions.append(suggestion);
        }
        emit suggestionsSucceeded(suggestions);
    });
}

void MapService::planRoute(double fromLat, double fromLng,
                           double toLat, double toLng,
                           const QString &mode)
{
    const quint64 serial = ++m_routeSerial;
    const QString routeMode = mode == QStringLiteral("walking")
        ? QStringLiteral("walking") : QStringLiteral("driving");
    if (!hasApiKey()) {
        emit routeFailed(QStringLiteral("未配置腾讯地图 Key"));
        return;
    }

    QUrl url(QStringLiteral("https://apis.map.qq.com/ws/direction/v1/%1")
                 .arg(routeMode));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("from"),
                       QStringLiteral("%1,%2").arg(fromLat, 0, 'f', 8)
                           .arg(fromLng, 0, 'f', 8));
    query.addQueryItem(QStringLiteral("to"),
                       QStringLiteral("%1,%2").arg(toLat, 0, 'f', 8)
                           .arg(toLng, 0, 'f', 8));
    query.addQueryItem(QStringLiteral("key"), m_apiKey);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setTransferTimeout(kRequestTimeoutMs);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("charging-platform/0.1"));
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, serial, routeMode] {
        const QByteArray payload = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        const QString networkMessage = reply->errorString();
        reply->deleteLater();

        if (serial != m_routeSerial)
            return;
        if (networkError != QNetworkReply::NoError) {
            emit routeFailed(QStringLiteral("路线规划请求失败：%1").arg(networkMessage));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            emit routeFailed(QStringLiteral("腾讯地图返回的路线数据格式无效"));
            return;
        }
        const QJsonObject root = document.object();
        const int status = root.value(QStringLiteral("status")).toInt(-1);
        if (status != 0) {
            emit routeFailed(QStringLiteral("路线规划失败（%1）：%2")
                                 .arg(status)
                                 .arg(root.value(QStringLiteral("message")).toString()));
            return;
        }

        const QJsonArray routes = root.value(QStringLiteral("result"))
                                      .toObject().value(QStringLiteral("routes")).toArray();
        if (routes.isEmpty()) {
            emit routeFailed(QStringLiteral("未找到可用路线"));
            return;
        }
        const QJsonObject route = routes.first().toObject();
        const QJsonArray encoded = route.value(QStringLiteral("polyline")).toArray();
        QVector<double> coordinates;
        coordinates.reserve(encoded.size());
        for (const QJsonValue &value : encoded)
            coordinates.append(value.toDouble());
        for (int i = 2; i < coordinates.size(); ++i)
            coordinates[i] = coordinates[i - 2] + coordinates[i] / 1000000.0;

        QVariantList path;
        for (int i = 0; i + 1 < coordinates.size(); i += 2) {
            QVariantMap point;
            point.insert(QStringLiteral("lat"), coordinates[i]);
            point.insert(QStringLiteral("lng"), coordinates[i + 1]);
            path.append(point);
        }
        if (path.size() < 2) {
            emit routeFailed(QStringLiteral("腾讯地图未返回有效路线坐标"));
            return;
        }
        emit routeSucceeded(routeMode, path,
                            route.value(QStringLiteral("distance")).toInt(),
                            route.value(QStringLiteral("duration")).toInt());
    });
}

QString MapService::loadConfiguredApiKey() const
{
    const QString environmentKey =
        qEnvironmentVariable("TENCENT_MAP_KEY").trimmed();
    if (!environmentKey.isEmpty())
        return environmentKey;

    QDir directory = QDir::current();
    for (int level = 0; level < 4; ++level) {
        const QString key = keyFromFile(directory.filePath(QStringLiteral("map.ini")));
        if (!key.isEmpty())
            return key;
        if (!directory.cdUp())
            break;
    }

    directory = QDir(QCoreApplication::applicationDirPath());
    for (int level = 0; level < 4; ++level) {
        const QString key = keyFromFile(directory.filePath(QStringLiteral("map.ini")));
        if (!key.isEmpty())
            return key;
        if (!directory.cdUp())
            break;
    }
    return {};
}
