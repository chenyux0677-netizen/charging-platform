#include "StationListPage.h"

#include "common/AppContext.h"
#include "common/WidgetUtil.h"
#include "user/services/MapService.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPair>
#include <QTimer>
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

QWidget *makeStationCard(const DataRow &station, int total, int free,
                         const QString &price, double distance)
{
    auto *card = new QWidget;
    card->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto *name = new QLabel(station.value(QStringLiteral("name")).toString(), card);
    name->setObjectName(QStringLiteral("stationCardName"));
    auto *distanceLabel = new QLabel(
        QStringLiteral("%1 km").arg(distance, 0, 'f', 1), card);
    distanceLabel->setObjectName(QStringLiteral("stationDistanceLabel"));

    auto *header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    header->addWidget(name, 1);
    header->addWidget(distanceLabel);

    auto *address = new QLabel(station.value(QStringLiteral("address")).toString(), card);
    address->setObjectName(QStringLiteral("stationCardAddress"));

    auto *availability = new QLabel(
        QStringLiteral("%1 空闲 / %2 个").arg(free).arg(total), card);
    availability->setObjectName(free > 0
                                    ? QStringLiteral("stationAvailableLabel")
                                    : QStringLiteral("stationUnavailableLabel"));
    auto *priceLabel = new QLabel(price, card);
    priceLabel->setObjectName(QStringLiteral("stationPriceLabel"));

    auto *summary = new QHBoxLayout;
    summary->setContentsMargins(0, 0, 0, 0);
    summary->addWidget(availability);
    summary->addStretch(1);
    summary->addWidget(priceLabel);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(5);
    layout->addLayout(header);
    layout->addWidget(address);
    layout->addLayout(summary);
    return card;
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
    m_addressEdit = new QLineEdit(this);
    m_addressEdit->setObjectName(QStringLiteral("addressEdit"));
    m_addressEdit->setPlaceholderText(QStringLiteral("全国搜索地点关键词或地址，选择候选"));

    m_locationStatusLabel = new QLabel(QStringLiteral("当前位置：模拟定位·北京海淀"), this);
    m_locationStatusLabel->setObjectName(QStringLiteral("locationStatusLabel"));
    m_locationStatusLabel->setWordWrap(true);

    m_suggestionList = new QListWidget(this);
    m_suggestionList->setObjectName(QStringLiteral("suggestionList"));
    m_suggestionList->hide();

    m_currentLat = m_regionCombo->itemData(0, Qt::UserRole).toDouble();
    m_currentLng = m_regionCombo->itemData(0, Qt::UserRole + 1).toDouble();
    m_mapService = new MapService(this);
    m_suggestionTimer = new QTimer(this);
    m_suggestionTimer->setSingleShot(true);
    m_suggestionTimer->setInterval(500);

    m_stationList = new QListWidget(this);
    m_stationList->setObjectName(QStringLiteral("stationList"));

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(m_regionCombo);
    layout->addWidget(m_addressEdit);
    layout->addWidget(m_locationStatusLabel);
    layout->addWidget(m_stationList, 1);

    connect(m_regionCombo, &QComboBox::currentIndexChanged,
            this, &StationListPage::onRegionChanged);
    connect(m_addressEdit, &QLineEdit::textEdited, this, [this](const QString &text) {
        m_suggestionTimer->stop();
        if (text.trimmed().size() < 2) {
            m_mapService->suggest(QString());
            return;
        }
        m_suggestionTimer->start();
    });
    connect(m_suggestionTimer, &QTimer::timeout,
            this, &StationListPage::requestSuggestions);
    connect(m_suggestionList, &QListWidget::itemClicked,
            this, &StationListPage::useSelectedSuggestion);
    connect(m_suggestionList, &QListWidget::itemActivated,
            this, &StationListPage::useSelectedSuggestion);
    connect(m_stationList, &QListWidget::itemClicked,
            this, &StationListPage::onItemClicked);

    connect(m_mapService, &MapService::suggestionsSucceeded, this,
            [this](const QVariantList &suggestions) {
        m_suggestionList->clear();
        for (const QVariant &value : suggestions) {
            const QVariantMap suggestion = value.toMap();
            const QString title = suggestion.value(QStringLiteral("title")).toString();
            const QString address = suggestion.value(QStringLiteral("address")).toString();
            auto *item = new QListWidgetItem(
                address.isEmpty() ? title : QStringLiteral("%1\n%2").arg(title, address));
            item->setData(Qt::UserRole, suggestion);
            m_suggestionList->addItem(item);
        }
        WidgetUtil::showSuggestionPopup(m_suggestionList, m_addressEdit);
    });
    connect(m_mapService, &MapService::suggestionsFailed, this,
            [this](const QString &message) {
        m_suggestionList->clear();
        auto *item = new QListWidgetItem(message);
        item->setFlags(Qt::NoItemFlags);
        m_suggestionList->addItem(item);
        WidgetUtil::showSuggestionPopup(m_suggestionList, m_addressEdit);
    });

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

    const double myLat = m_currentLat;
    const double myLng = m_currentLng;

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
        auto *item = new QListWidgetItem;
        item->setData(Qt::UserRole, r.station.value(QStringLiteral("id")).toLongLong());
        item->setSizeHint(QSize(0, 98));
        m_stationList->addItem(item);
        m_stationList->setItemWidget(
            item, makeStationCard(r.station, r.total, r.free, priceText, r.dist));
    }
}

QString StationListPage::currentLocationName() const
{
    const QString address = m_addressEdit->text().trimmed();
    return address.isEmpty() ? m_regionCombo->currentText() : address;
}

void StationListPage::onRegionChanged()
{
    const int index = m_regionCombo->currentIndex();
    m_currentLat = m_regionCombo->itemData(index, Qt::UserRole).toDouble();
    m_currentLng = m_regionCombo->itemData(index, Qt::UserRole + 1).toDouble();
    m_locationStatusLabel->setText(
        QStringLiteral("当前位置：%1").arg(m_regionCombo->currentText()));
    refresh();
}

void StationListPage::requestSuggestions()
{
    m_mapService->suggest(m_addressEdit->text());
}

void StationListPage::useSelectedSuggestion()
{
    QListWidgetItem *item = m_suggestionList->currentItem();
    if (!item || !(item->flags() & Qt::ItemIsEnabled))
        return;
    const QVariantMap suggestion = item->data(Qt::UserRole).toMap();
    m_currentLat = suggestion.value(QStringLiteral("lat")).toDouble();
    m_currentLng = suggestion.value(QStringLiteral("lng")).toDouble();
    const QString title = suggestion.value(QStringLiteral("title")).toString();
    m_addressEdit->setText(title);
    m_locationStatusLabel->setText(
        QStringLiteral("当前位置：%1（%2, %3）")
            .arg(title)
            .arg(m_currentLat, 0, 'f', 6)
            .arg(m_currentLng, 0, 'f', 6));
    m_suggestionList->hide();
    refresh();
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
