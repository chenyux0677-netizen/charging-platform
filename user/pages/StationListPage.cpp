#include "StationListPage.h"

#include "common/AppContext.h"

#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QPair>
#include <QtMath>
#include <QVBoxLayout>
#include <QVector>

#include <algorithm>

namespace {

// 球面距离(haversine),返回公里数
double distanceKm(double lat1, double lng1, double lat2, double lng2)
{
    constexpr double kEarthRadiusKm = 6371.0;
    const double dLat = qDegreesToRadians(lat2 - lat1);
    const double dLng = qDegreesToRadians(lng2 - lng1);
    const double a = qSin(dLat / 2.0) * qSin(dLat / 2.0)
        + qCos(qDegreesToRadians(lat1)) * qCos(qDegreesToRadians(lat2))
          * qSin(dLng / 2.0) * qSin(dLng / 2.0);
    const double c = 2.0 * qAtan2(qSqrt(a), qSqrt(1.0 - a));
    return kEarthRadiusKm * c;
}

} // namespace

StationListPage::StationListPage(QWidget *parent)
    : QWidget(parent)
{
    auto *title = new QLabel(QStringLiteral("附近充电站"), this);
    title->setObjectName(QStringLiteral("pageTitleLabel"));
    title->setAlignment(Qt::AlignCenter);

    m_regionCombo = new QComboBox(this);
    m_regionCombo->setObjectName(QStringLiteral("regionCombo"));
    // 模拟定位:每个城市区域带一组"当前 GPS 坐标"(UserRole=纬度, UserRole+1=经度)
    const QVector<QPair<QString, QPair<double, double>>> regions = {
        { QStringLiteral("模拟定位·北京海淀"), { 39.959, 116.298 } },
        { QStringLiteral("模拟定位·上海浦东"), { 31.222, 121.544 } },
        { QStringLiteral("模拟定位·广州天河"), { 23.127, 113.359 } },
    };
    for (const auto &r : regions) {
        m_regionCombo->addItem(r.first);
        const int idx = m_regionCombo->count() - 1;
        m_regionCombo->setItemData(idx, r.second.first, Qt::UserRole);
        m_regionCombo->setItemData(idx, r.second.second, Qt::UserRole + 1);
    }

    m_stationList = new QListWidget(this);
    m_stationList->setObjectName(QStringLiteral("stationList"));

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(m_regionCombo);
    layout->addWidget(m_stationList, 1);

    connect(m_regionCombo, &QComboBox::currentIndexChanged,
            this, &StationListPage::refresh);
    connect(m_stationList, &QListWidget::itemClicked,
            this, &StationListPage::onItemClicked);

    // 数据变更(管理员新增电站 / 充电进度变化) → 自动刷新
    if (DataSource *ds = AppContext::instance()->dataSource()) {
        connect(ds, &DataSource::dataChanged, this, [this](const QString &table) {
            if (table == QStringLiteral("charging_stations")
                || table == QStringLiteral("charging_piles"))
                refresh();
        });
    }
}

void StationListPage::refresh()
{
    DataSource *ds = AppContext::instance()->dataSource();
    m_stationList->clear();
    if (!ds)
        return;

    const double myLat = m_regionCombo->itemData(m_regionCombo->currentIndex(), Qt::UserRole).toDouble();
    const double myLng = m_regionCombo->itemData(m_regionCombo->currentIndex(), Qt::UserRole + 1).toDouble();

    const QueryResult stations = ds->query(QStringLiteral("charging_stations"));
    if (stations.isEmpty()) {
        auto *empty = new QListWidgetItem(QStringLiteral("暂无充电站,可在管理员端新增"));
        empty->setFlags(Qt::NoItemFlags);
        m_stationList->addItem(empty);
        return;
    }

    struct Row {
        double dist;
        DataRow station;
        int total;
        int free;
        double minPrice;
        double maxPrice;
    };
    QVector<Row> rows;
    rows.reserve(stations.size());

    for (const DataRow &st : stations) {
        const qlonglong sid = st.value(QStringLiteral("id")).toLongLong();
        const QueryResult piles = ds->query(QStringLiteral("charging_piles"), {},
                                            QStringLiteral("station_id = ?"),
                                            QVariantList{sid});
        int total = piles.size();
        int free = 0;
        double minPrice = 0.0;
        double maxPrice = 0.0;
        bool firstPrice = true;
        for (const DataRow &p : piles) {
            if (p.value(QStringLiteral("status")).toString() == QStringLiteral("空闲"))
                ++free;
            const double price = p.value(QStringLiteral("price_per_kwh")).toDouble();
            if (firstPrice) {
                minPrice = maxPrice = price;
                firstPrice = false;
            } else {
                minPrice = qMin(minPrice, price);
                maxPrice = qMax(maxPrice, price);
            }
        }

        Row r;
        r.dist = distanceKm(myLat, myLng,
                            st.value(QStringLiteral("lat")).toDouble(),
                            st.value(QStringLiteral("lng")).toDouble());
        r.station = st;
        r.total = total;
        r.free = free;
        r.minPrice = minPrice;
        r.maxPrice = maxPrice;
        rows << r;
    }

    // 按距离升序
    std::sort(rows.begin(), rows.end(),
              [](const Row &a, const Row &b) { return a.dist < b.dist; });

    for (const Row &r : rows) {
        QString priceText = QStringLiteral("暂无报价");
        if (r.total > 0) {
            priceText = qFuzzyCompare(r.minPrice + 1.0, r.maxPrice + 1.0)
                ? QStringLiteral("%1 元/度").arg(r.minPrice, 0, 'f', 2)
                : QStringLiteral("%1~%2 元/度")
                      .arg(r.minPrice, 0, 'f', 2)
                      .arg(r.maxPrice, 0, 'f', 2);
        }
        const QString text = QStringLiteral("%1\n价格:%2  电桩:%3 总/%4 空闲  距离:%5 km")
            .arg(r.station.value(QStringLiteral("name")).toString())
            .arg(priceText)
            .arg(r.total)
            .arg(r.free)
            .arg(r.dist, 0, 'f', 1);
        auto *item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, r.station.value(QStringLiteral("id")).toLongLong());
        m_stationList->addItem(item);
    }
}

void StationListPage::onItemClicked()
{
    QListWidgetItem *item = m_stationList->currentItem();
    if (!item)
        return;
    const qlonglong id = item->data(Qt::UserRole).toLongLong();
    if (id <= 0)
        return;

    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds)
        return;
    const QueryResult st = ds->query(QStringLiteral("charging_stations"), {},
                                     QStringLiteral("id = ?"), QVariantList{id});
    if (st.isEmpty())
        return;
    emit stationClicked(st.first());
}
