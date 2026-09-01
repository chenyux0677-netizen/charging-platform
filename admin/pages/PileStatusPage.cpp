#include "PileStatusPage.h"

#include "common/AppContext.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidgetItem>
#include <QVBoxLayout>

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
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({QStringLiteral("桩号"),
                                        QStringLiteral("所属站"),
                                        QStringLiteral("类型"),
                                        QStringLiteral("功率(kW)"),
                                        QStringLiteral("状态"),
                                        QStringLiteral("充电次数"),
                                        QStringLiteral("充电时长(分)")});
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
            // 充电进度变化 / 管理员改状态都会触发刷新
            if (table == QStringLiteral("charging_piles")
                || table == QStringLiteral("charging_stations"))
                refresh();
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
        put(1, names.value(p.value(QStringLiteral("station_id")).toLongLong()));
        put(2, p.value(QStringLiteral("type")));
        put(3, QString::number(p.value(QStringLiteral("power_kw")).toDouble(), 'f', 1));
        put(4, p.value(QStringLiteral("status")));
        put(5, p.value(QStringLiteral("charge_count")));
        put(6, p.value(QStringLiteral("charge_duration_min")));
    }
}
