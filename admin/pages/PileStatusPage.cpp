#include "PileStatusPage.h"

#include "common/AppContext.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {
void setProgressCells(QTableWidget *table, int row, const DataRow &order,
                      const QHash<qlonglong, QString> &nicknames)
{
    const auto setText = [&](int col, const QString &text) {
        QTableWidgetItem *item = table->item(row, col);
        if (!item) {
            item = new QTableWidgetItem;
            item->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col, item);
        }
        item->setText(text);
    };

    if (order.isEmpty()) {
        for (int col = 7; col <= 10; ++col)
            setText(col, QStringLiteral("—"));
        return;
    }
    setText(7, nicknames.value(order.value(QStringLiteral("user_id")).toLongLong(),
                               QStringLiteral("—")));
    setText(8, QString::number(order.value(QStringLiteral("duration_min")).toLongLong()));
    setText(9, QString::number(order.value(QStringLiteral("energy_kwh")).toDouble(), 'f', 2));
    setText(10, QString::number(order.value(QStringLiteral("amount")).toDouble(), 'f', 2));
}
}

PileStatusPage::PileStatusPage(QWidget *parent)
    : QWidget(parent)
{
    auto *title = new QLabel(QStringLiteral("充电桩状态"), this);
    title->setObjectName(QStringLiteral("pageTitleLabel"));

    auto *summaryRow = new QHBoxLayout;
    const auto addSummaryCard = [this, summaryRow](const QString &cardName,
                                                    const QString &caption,
                                                    const QString &valueName) {
        auto *card = new QWidget(this);
        card->setObjectName(cardName);
        auto *captionLabel = new QLabel(caption, card);
        captionLabel->setObjectName(QStringLiteral("pileStatusSummaryCaption"));
        auto *valueLabel = new QLabel(QStringLiteral("—"), card);
        valueLabel->setObjectName(valueName);
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(14, 10, 14, 10);
        cardLayout->setSpacing(3);
        cardLayout->addWidget(captionLabel);
        cardLayout->addWidget(valueLabel);
        summaryRow->addWidget(card, 1);
        return valueLabel;
    };
    m_totalValue = addSummaryCard(QStringLiteral("pileSummaryTotal"),
                                  QStringLiteral("电桩总数"),
                                  QStringLiteral("pileTotalValue"));
    m_freeValue = addSummaryCard(QStringLiteral("pileSummaryFree"),
                                 QStringLiteral("空闲"),
                                 QStringLiteral("pileFreeValue"));
    m_busyValue = addSummaryCard(QStringLiteral("pileSummaryBusy"),
                                 QStringLiteral("使用中"),
                                 QStringLiteral("pileBusyValue"));
    m_faultValue = addSummaryCard(QStringLiteral("pileSummaryFault"),
                                  QStringLiteral("故障"),
                                  QStringLiteral("pileFaultValue"));

    auto *filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(QStringLiteral("按状态筛选:"), this));
    m_statusFilter = new QComboBox(this);
    m_statusFilter->addItems({QStringLiteral("全部"),
                              QStringLiteral("空闲"),
                              QStringLiteral("使用中"),
                              QStringLiteral("故障")});
    filterRow->addWidget(m_statusFilter);
    filterRow->addStretch(1);

    auto *refreshBtn = new QPushButton(QStringLiteral("刷新"), this);
    filterRow->addWidget(refreshBtn);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(11);
    m_table->setHorizontalHeaderLabels({QStringLiteral("桩号"),
                                        QStringLiteral("所属站"),
                                        QStringLiteral("类型"),
                                        QStringLiteral("功率(kW)"),
                                        QStringLiteral("状态"),
                                        QStringLiteral("充电次数"),
                                        QStringLiteral("充电时长(分)"),
                                        QStringLiteral("充电用户"),
                                        QStringLiteral("本次时长(分)"),
                                        QStringLiteral("本次电量(kWh)"),
                                        QStringLiteral("本次费用(元)")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    QHeaderView *header = m_table->horizontalHeader();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionResizeMode(1, QHeaderView::Stretch);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addLayout(summaryRow);
    layout->addLayout(filterRow);
    layout->addWidget(m_table, 1);

    connect(m_statusFilter, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &PileStatusPage::refresh);
    connect(refreshBtn, &QPushButton::clicked, this, &PileStatusPage::refresh);

    if (DataSource *ds = AppContext::instance()->dataSource()) {
        connect(ds, &DataSource::dataChanged, this, [this](const QString &table) {
            // 充电进度变化 / 管理员改状态都会触发刷新
            if (table == QStringLiteral("charging_piles")
                || table == QStringLiteral("charging_stations"))
                refresh();
            else if (table == QStringLiteral("charging_progress"))
                refreshActiveProgress();
        });
    }

    refresh();
}

void PileStatusPage::refresh()
{
    m_table->setRowCount(0);
    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds)
        return;

    // 站名映射(id -> 名称)
    QHash<qlonglong, QString> names;
    const QueryResult stations = ds->query(QStringLiteral("charging_stations"));
    for (const DataRow &st : stations)
        names.insert(st.value(QStringLiteral("id")).toLongLong(),
                     st.value(QStringLiteral("name")).toString());

    QHash<qlonglong, QString> nicknames;
    const QueryResult users = ds->query(QStringLiteral("users"));
    for (const DataRow &user : users)
        nicknames.insert(user.value(QStringLiteral("id")).toLongLong(),
                         user.value(QStringLiteral("nickname")).toString());
    QHash<qlonglong, DataRow> activeByPile;
    const QueryResult activeOrders =
        ds->query(QStringLiteral("orders"), {}, QStringLiteral("status = '充电中'"));
    for (const DataRow &order : activeOrders)
        activeByPile.insert(order.value(QStringLiteral("pile_id")).toLongLong(), order);

    const QString filter = m_statusFilter->currentText();
    const QueryResult piles = ds->query(QStringLiteral("charging_piles"));

    int freeCount = 0;
    int busyCount = 0;
    int faultCount = 0;
    for (const DataRow &pile : piles) {
        const QString status = pile.value(QStringLiteral("status")).toString();
        if (status == QStringLiteral("空闲"))
            ++freeCount;
        else if (status == QStringLiteral("使用中"))
            ++busyCount;
        else if (status == QStringLiteral("故障"))
            ++faultCount;
    }
    const int total = piles.size();
    const auto statusText = [total](int count) {
        const double percent = total > 0 ? count * 100.0 / total : 0.0;
        return QStringLiteral("%1 根 · %2%").arg(count).arg(percent, 0, 'f', 1);
    };
    m_totalValue->setText(QStringLiteral("%1 根").arg(total));
    m_freeValue->setText(statusText(freeCount));
    m_busyValue->setText(statusText(busyCount));
    m_faultValue->setText(statusText(faultCount));

    QVector<DataRow> rows;
    rows.reserve(piles.size());
    for (const DataRow &p : piles) {
        if (filter != QStringLiteral("全部")
            && p.value(QStringLiteral("status")).toString() != filter)
            continue;
        rows << p;
    }

    m_table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const DataRow &p = rows.at(i);
        const auto put = [&](int col, const QVariant &v) {
            auto *item = new QTableWidgetItem(v.toString());
            item->setTextAlignment(Qt::AlignCenter);
            m_table->setItem(i, col, item);
        };
        put(0, p.value(QStringLiteral("code")));
        m_table->item(i, 0)->setData(Qt::UserRole, p.value(QStringLiteral("id")));
        put(1, names.value(p.value(QStringLiteral("station_id")).toLongLong()));
        put(2, p.value(QStringLiteral("type")));
        put(3, QString::number(p.value(QStringLiteral("power_kw")).toDouble(), 'f', 1));
        put(4, p.value(QStringLiteral("status")));
        put(5, p.value(QStringLiteral("charge_count")));
        put(6, p.value(QStringLiteral("charge_duration_min")));
        DataRow activeOrder;
        if (p.value(QStringLiteral("status")).toString() == QStringLiteral("使用中"))
            activeOrder = activeByPile.value(p.value(QStringLiteral("id")).toLongLong());
        setProgressCells(m_table, i, activeOrder, nicknames);
    }
}

void PileStatusPage::refreshActiveProgress()
{
    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds || m_table->rowCount() == 0)
        return;

    QHash<qlonglong, QString> nicknames;
    const QueryResult users = ds->query(QStringLiteral("users"));
    for (const DataRow &user : users)
        nicknames.insert(user.value(QStringLiteral("id")).toLongLong(),
                         user.value(QStringLiteral("nickname")).toString());
    QHash<qlonglong, DataRow> activeByPile;
    const QueryResult activeOrders =
        ds->query(QStringLiteral("orders"), {}, QStringLiteral("status = '充电中'"));
    for (const DataRow &order : activeOrders)
        activeByPile.insert(order.value(QStringLiteral("pile_id")).toLongLong(), order);

    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem *codeItem = m_table->item(row, 0);
        QTableWidgetItem *statusItem = m_table->item(row, 4);
        if (!codeItem || !statusItem
            || statusItem->text() != QStringLiteral("使用中"))
            continue;
        setProgressCells(m_table, row,
                         activeByPile.value(codeItem->data(Qt::UserRole).toLongLong()),
                         nicknames);
    }
}
