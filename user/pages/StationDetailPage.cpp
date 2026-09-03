#include "StationDetailPage.h"

#include "common/AppContext.h"

#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

StationDetailPage::StationDetailPage(QWidget *parent)
    : QWidget(parent)
{
    m_backButton = new QPushButton(QStringLiteral("← 返回"), this);
    m_backButton->setObjectName(QStringLiteral("backButton"));

    m_stationNameLabel = new QLabel(this);
    m_stationNameLabel->setObjectName(QStringLiteral("stationNameLabel"));
    m_stationNameLabel->setWordWrap(true);

    m_stationAddrLabel = new QLabel(this);
    m_stationAddrLabel->setObjectName(QStringLiteral("stationAddrLabel"));
    m_stationAddrLabel->setWordWrap(true);

    m_navigationButton = new QPushButton(QStringLiteral("导航到这里"), this);
    m_navigationButton->setObjectName(QStringLiteral("navigationButton"));
    m_navigationButton->setEnabled(false);

    m_pileList = new QListWidget(this);
    m_pileList->setObjectName(QStringLiteral("pileList"));

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_backButton);
    layout->addWidget(m_stationNameLabel);
    layout->addWidget(m_stationAddrLabel);
    layout->addWidget(m_navigationButton);
    layout->addWidget(m_pileList, 1);

    connect(m_backButton, &QPushButton::clicked, this, &StationDetailPage::backRequested);
    connect(m_navigationButton, &QPushButton::clicked, this, [this] {
        if (!m_station.isEmpty())
            emit navigationRequested(m_station);
    });
    connect(m_pileList, &QListWidget::itemClicked, this, &StationDetailPage::onPileClicked);

    // 充电进度变化(占用/空闲) → 刷新桩状态
    if (DataSource *ds = AppContext::instance()->dataSource()) {
        connect(ds, &DataSource::dataChanged, this, [this](const QString &table) {
            if (table == QStringLiteral("charging_piles") && !m_station.isEmpty())
                refreshPiles();
        });
    }
}

void StationDetailPage::setStation(const DataRow &station)
{
    m_station = station;
    m_stationNameLabel->setText(station.value(QStringLiteral("name")).toString());
    m_stationAddrLabel->setText(QStringLiteral("地址:%1")
                                    .arg(station.value(QStringLiteral("address")).toString()));
    m_navigationButton->setEnabled(
        !station.value(QStringLiteral("lat")).isNull()
        && !station.value(QStringLiteral("lng")).isNull());
    refreshPiles();
}

void StationDetailPage::refreshPiles()
{
    m_pileList->clear();
    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds || m_station.isEmpty())
        return;

    const qlonglong sid = m_station.value(QStringLiteral("id")).toLongLong();
    const QueryResult piles = ds->query(QStringLiteral("charging_piles"), {},
                                        QStringLiteral("station_id = ?"),
                                        QVariantList{sid});
    if (piles.isEmpty()) {
        auto *empty = new QListWidgetItem(QStringLiteral("该站暂无充电桩"));
        empty->setFlags(Qt::NoItemFlags);
        m_pileList->addItem(empty);
        return;
    }
    for (const DataRow &p : piles) {
        const QString text = QStringLiteral("%1  |  %2  |  %3 kW  |  %4")
            .arg(p.value(QStringLiteral("code")).toString())
            .arg(p.value(QStringLiteral("type")).toString())
            .arg(p.value(QStringLiteral("power_kw")).toDouble(), 0, 'f', 0)
            .arg(p.value(QStringLiteral("status")).toString());
        auto *item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, p.value(QStringLiteral("id")).toLongLong());
        item->setData(Qt::UserRole + 1, p.value(QStringLiteral("status")).toString());
        m_pileList->addItem(item);
    }
}

void StationDetailPage::onPileClicked()
{
    QListWidgetItem *item = m_pileList->currentItem();
    if (!item)
        return;
    const qlonglong id = item->data(Qt::UserRole).toLongLong();
    if (id <= 0)
        return;
    if (item->data(Qt::UserRole + 1).toString() != QStringLiteral("空闲")) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("该充电桩当前不可用,请选择其他空闲桩。"));
        return;
    }

    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds)
        return;
    const QueryResult pile = ds->query(QStringLiteral("charging_piles"), {},
                                       QStringLiteral("id = ?"), QVariantList{id});
    if (pile.isEmpty())
        return;
    emit pileChosen(pile.first());
}
