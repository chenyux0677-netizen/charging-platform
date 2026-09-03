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
// 填写第 row 行的 4 个实时进度格(7~10 列):order 为空 → 全部置 "—"。
// 复用已存在的单元格(无则新建),只改文本,不触发整表重建 → 无闪烁。
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
        for (int c = 7; c <= 10; ++c)
            setText(c, QStringLiteral("—"));
        return;
    }
    setText(7, nicknames.value(order.value(QStringLiteral("user_id")).toLongLong(),
                               QStringLiteral("—")));
    setText(8, QString::number(order.value(QStringLiteral("duration_min")).toLongLong()));
    setText(9, QString::number(order.value(QStringLiteral("energy_kwh")).toDouble(), 'f', 2));
    setText(10, QString::number(order.value(QStringLiteral("amount")).toDouble(), 'f', 2));
}
} // namespace

PileStatusPage::PileStatusPage(QWidget *parent)
    : QWidget(parent)
{
    auto *title = new QLabel(QStringLiteral("充电桩状态"), this);
    title->setObjectName(QStringLiteral("pageTitleLabel"));

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
                                        // 实时进度(仅使用中的桩有值,其余为 —)
                                        QStringLiteral("充电用户"),
                                        QStringLiteral("本次时长(分)"),
                                        QStringLiteral("本次电量(kWh)"),
                                        QStringLiteral("本次费用(元)")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addLayout(filterRow);
    layout->addWidget(m_table, 1);

    connect(m_statusFilter, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &PileStatusPage::refresh);
    connect(refreshBtn, &QPushButton::clicked, this, &PileStatusPage::refresh);

    if (DataSource *ds = AppContext::instance()->dataSource()) {
        connect(ds, &DataSource::dataChanged, this, [this](const QString &table) {
            if (table == QStringLiteral("charging_piles")
                || table == QStringLiteral("charging_stations"))
                refresh();               // 桩/站状态真变(开始/结束充电、增删改):整表重建
            else if (table == QStringLiteral("orders"))
                refreshActiveProgress(); // 充电中每拍进度上报:原地更新进度格,避免整表每秒闪烁
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

    // 用户昵称映射 + "充电中"订单按桩索引(桩级同时至多一笔活动单,由部分唯一索引保证)
    QHash<qlonglong, QString> nicknames;
    const QueryResult users = ds->query(QStringLiteral("users"));
    for (const DataRow &u : users)
        nicknames.insert(u.value(QStringLiteral("id")).toLongLong(),
                         u.value(QStringLiteral("nickname")).toString());
    QHash<qlonglong, DataRow> activeByPile;
    const QueryResult activeOrders =
        ds->query(QStringLiteral("orders"), {}, QStringLiteral("status = '充电中'"));
    for (const DataRow &o : activeOrders)
        activeByPile.insert(o.value(QStringLiteral("pile_id")).toLongLong(), o);

    const QString filter = m_statusFilter->currentText();
    const QueryResult piles = ds->query(QStringLiteral("charging_piles"));

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
        // 行内记住桩 id:充电心跳做原地更新时据此定位"在充"行,无需整表重建
        if (QTableWidgetItem *codeItem = m_table->item(i, 0))
            codeItem->setData(Qt::UserRole, p.value(QStringLiteral("id")));
        put(1, names.value(p.value(QStringLiteral("station_id")).toLongLong()));
        put(2, p.value(QStringLiteral("type")));
        put(3, QString::number(p.value(QStringLiteral("power_kw")).toDouble(), 'f', 1));
        put(4, p.value(QStringLiteral("status")));
        put(5, p.value(QStringLiteral("charge_count")));
        put(6, p.value(QStringLiteral("charge_duration_min")));
        // 实时进度:只对"使用中"且有活动订单的桩填值,否则占位 —
        DataRow order;
        if (p.value(QStringLiteral("status")).toString() == QStringLiteral("使用中")) {
            const auto activeIt =
                activeByPile.constFind(p.value(QStringLiteral("id")).toLongLong());
            if (activeIt != activeByPile.cend())
                order = activeIt.value();
        }
        setProgressCells(m_table, i, order, nicknames);
    }
}

void PileStatusPage::refreshActiveProgress()
{
    if (m_table->rowCount() == 0)
        return;
    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds)
        return;

    // 昵称映射 + "充电中"订单按桩索引(只读查询,不产生广播,无嵌套刷新)
    QHash<qlonglong, QString> nicknames;
    const QueryResult users = ds->query(QStringLiteral("users"));
    for (const DataRow &u : users)
        nicknames.insert(u.value(QStringLiteral("id")).toLongLong(),
                         u.value(QStringLiteral("nickname")).toString());
    QHash<qlonglong, DataRow> activeByPile;
    const QueryResult activeOrders =
        ds->query(QStringLiteral("orders"), {}, QStringLiteral("status = '充电中'"));
    for (const DataRow &o : activeOrders)
        activeByPile.insert(o.value(QStringLiteral("pile_id")).toLongLong(), o);

    // 只原地改写当前"使用中"行的进度格;非使用中行(含结算/增删引发的整表重建间隙)
    // 一律跳过——整表刷新由 charging_piles/charging_stations 广播负责。
    for (int i = 0; i < m_table->rowCount(); ++i) {
        QTableWidgetItem *codeItem = m_table->item(i, 0);
        QTableWidgetItem *statusItem = m_table->item(i, 4);
        if (!codeItem || !statusItem
            || statusItem->text() != QStringLiteral("使用中"))
            continue;
        const qlonglong pileId = codeItem->data(Qt::UserRole).toLongLong();
        const auto activeIt = activeByPile.constFind(pileId);
        setProgressCells(m_table, i,
                         activeIt == activeByPile.cend() ? DataRow() : activeIt.value(),
                         nicknames);
    }
}
