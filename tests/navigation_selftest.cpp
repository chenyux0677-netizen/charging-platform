#include "user/pages/NavigationPage.h"

#include <QApplication>
#include <QLabel>
#include <QTextStream>
#include <QTimer>
#include <QWebEnginePage>
#include <QWebEngineView>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QTextStream out(stdout);

    DataRow station;
    station.insert(QStringLiteral("name"), QStringLiteral("北京理工大学充电站"));
    station.insert(QStringLiteral("lat"), 39.959951);
    station.insert(QStringLiteral("lng"), 116.315227);

    NavigationPage page;
    page.resize(360, 640);
    page.showLocations(QStringLiteral("中关村南大街5号院7号楼"),
                       39.962336, 116.316066, station);
    page.show();

    QWebEngineView *webView = page.findChild<QWebEngineView *>(
        QStringLiteral("navigationWebView"));
    if (!webView) {
        out << "地图页面自检失败：未创建 QWebEngineView\n";
        return 1;
    }

    QObject::connect(webView, &QWebEngineView::loadFinished, &app,
                     [&, webView](bool ok) {
        if (!ok) {
            out << "地图页面自检失败：HTML 或外部脚本加载失败\n";
            app.exit(1);
            return;
        }
        QTimer::singleShot(1500, webView, [&, webView] {
            webView->page()->runJavaScript(
                QStringLiteral("JSON.stringify({map:typeof TMap,"
                               "route:typeof window.routePolyline,"
                               "children:document.getElementById('map').children.length})"),
                [&](const QVariant &result) {
                    const QString details = result.toString();
                    if (details.contains(QStringLiteral("\"map\":\"object\""))
                        && details.contains(QStringLiteral("\"route\":\"object\""))) {
                        out << "地图页面自检通过：地图与路线折线已初始化\n";
                        app.exit(0);
                    } else {
                        const QLabel *status = page.findChild<QLabel *>(
                            QStringLiteral("mapStatusLabel"));
                        out << "地图页面自检失败：未检测到地图或路线折线，JS="
                            << details << "，状态="
                            << (status ? status->text() : QString()) << "\n";
                        app.exit(1);
                    }
                });
        });
    });

    QTimer::singleShot(15000, &app, [&] {
        out << "地图页面自检失败：等待地图加载超时\n";
        app.exit(1);
    });
    return app.exec();
}
