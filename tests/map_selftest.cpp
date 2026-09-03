#include "user/services/MapService.h"

#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    const QString mode = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    const bool suggestionMode = mode == QStringLiteral("--suggest");
    const bool routeMode = mode == QStringLiteral("--route");
    const QString routeTravelMode = argc > 2
        ? QString::fromLocal8Bit(argv[2]) : QStringLiteral("driving");
    const int valueIndex = suggestionMode ? 2 : 1;
    const QString address = argc > valueIndex
        ? QString::fromLocal8Bit(argv[valueIndex])
        : (suggestionMode ? QStringLiteral("北京理工")
                          : QStringLiteral("北京市海淀区中关村南大街5号"));
    const QString region = argc > valueIndex + 1
        ? QString::fromLocal8Bit(argv[valueIndex + 1])
        : QStringLiteral("北京");

    MapService service;
    if (!service.hasApiKey()) {
        out << "地图自检失败：未配置 TENCENT_MAP_KEY 或 map.ini\n";
        return 1;
    }

    QObject::connect(&service, &MapService::geocodeSucceeded,
                     &app, [&](const QString &resolved, double lat, double lng) {
        out << "地图自检通过：" << resolved << "\n"
            << "纬度=" << QString::number(lat, 'f', 6)
            << " 经度=" << QString::number(lng, 'f', 6) << "\n";
        app.exit(0);
    });
    QObject::connect(&service, &MapService::requestFailed,
                     &app, [&](const QString &message) {
        out << "地图自检失败：" << message << "\n";
        app.exit(1);
    });
    QObject::connect(&service, &MapService::suggestionsSucceeded,
                     &app, [&](const QVariantList &suggestions) {
        if (suggestions.isEmpty()) {
            out << "地图自检失败：关键词未返回候选地点\n";
            app.exit(1);
            return;
        }
        const QVariantMap first = suggestions.first().toMap();
        out << "地点搜索自检通过：共 " << suggestions.size() << " 个候选\n"
            << "首项=" << first.value(QStringLiteral("title")).toString()
            << "，" << first.value(QStringLiteral("address")).toString() << "\n"
            << "纬度=" << QString::number(first.value(QStringLiteral("lat")).toDouble(), 'f', 6)
            << " 经度=" << QString::number(first.value(QStringLiteral("lng")).toDouble(), 'f', 6)
            << "\n";
        app.exit(0);
    });
    QObject::connect(&service, &MapService::suggestionsFailed,
                     &app, [&](const QString &message) {
        out << "地图自检失败：" << message << "\n";
        app.exit(1);
    });
    QObject::connect(&service, &MapService::routeSucceeded,
                     &app, [&](const QString &, const QVariantList &path,
                               int distanceMeters, int durationMinutes) {
        out << "路线规划自检通过：共 " << path.size() << " 个轨迹点\n"
            << "距离=" << distanceMeters << " 米"
            << " 时间=" << durationMinutes << " 分钟\n";
        app.exit(0);
    });
    QObject::connect(&service, &MapService::routeFailed,
                     &app, [&](const QString &message) {
        out << "路线规划自检失败：" << message << "\n";
        app.exit(1);
    });

    QTimer::singleShot(15000, &app, [&] {
        out << "地图自检失败：等待腾讯地图响应超时\n";
        app.exit(1);
    });
    if (routeMode)
        service.planRoute(39.962336, 116.316066,
                          39.959951, 116.315227, routeTravelMode);
    else if (suggestionMode)
        service.suggest(address, region);
    else
        service.geocode(address, region);
    return app.exec();
}
