#include "PileManagePage.h"

#include "common/AppContext.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidgetItem>
#include <QVBoxLayout>

PileManagePage::PileManagePage(QWidget *parent)
    : QWidget(parent)
{
    auto *title = new QLabel(QStringLiteral("充电桩管理"), this);
    title->setObjectName(QStringLiteral("pageTitleLabel"));

    auto *filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(QStringLiteral("按电站筛选:"), this));
    m_stationFilter = new QComboBox(this);
    filterRow->addWidget(m_stationFilter);
    filterRow->addStretch(1);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(9);
    m_table->setHorizontalHeaderLabels({QStringLiteral("ID"),
                                        QStringLiteral("桩号"),
                                        QStringLiteral("所属站"),
                                        QStringLiteral("类型"),
                                        QStringLiteral("功率(kW)"),
                                        QStringLiteral("单价(元/度)"),
                                        QStringLiteral("状态"),
                                        QStringLiteral("充电次数"),
                                        QStringLiteral("充电时长(分)")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);

    auto *addBtn = new QPushButton(QStringLiteral("新增电桩"), this);
    auto *editBtn = new QPushButton(QStringLiteral("修改"), this);
    auto *removeBtn = new QPushButton(QStringLiteral("删除"), this);
    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(addBtn);
    btnRow->addWidget(editBtn);
    btnRow->addWidget(removeBtn);
    btnRow->addStretch(1);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addLayout(filterRow);
    layout->addWidget(m_table, 1);
    layout->addLayout(btnRow);

    connect(addBtn, &QPushButton::clicked, this, &PileManagePage::onAdd);
    connect(editBtn, &QPushButton::clicked, this, &PileManagePage::onEdit);
    connect(removeBtn, &QPushButton::clicked, this, &PileManagePage::onRemove);
    connect(m_stationFilter, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &PileManagePage::refresh);

    if (DataSource *ds = AppContext::instance()->dataSource()) {
        connect(ds, &DataSource::dataChanged, this, [this](const QString &table) {
            // 桩或电站变了都刷新(电站列表变化会影响筛选框与所属站列)
            if (table == QStringLiteral("charging_piles")
                || table == QStringLiteral("charging_stations"))
                refresh();
        });
    }

    refresh();
}

QHash<qlonglong, QString> PileManagePage::stationNames() const
{
    QHash<qlonglong, QString> names;
    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds)
        return names;
    const QueryResult stations = ds->query(QStringLiteral("charging_stations"));
    for (const DataRow &st : stations)
        names.insert(st.value(QStringLiteral("id")).toLongLong(),
                     st.value(QStringLiteral("name")).toString());
    return names;
}

void PileManagePage::refresh()
{
    // 站名映射先存到局部变量(不能直接在临时返回值上迭代,会产生悬垂迭代器)
    const QHash<qlonglong, QString> names = stationNames();

    // 重建筛选下拉框(保留当前选中)
    const qlonglong prevFilter = m_stationFilter->currentData().toLongLong();
    m_stationFilter->blockSignals(true);
    m_stationFilter->clear();
    m_stationFilter->addItem(QStringLiteral("全部电站"), QVariant::fromValue(qlonglong(0)));
    for (auto it = names.cbegin(); it != names.cend(); ++it)
        m_stationFilter->addItem(it.value(), QVariant::fromValue(it.key()));
    const int prevIdx = m_stationFilter->findData(QVariant::fromValue(prevFilter));
    m_stationFilter->setCurrentIndex(prevIdx >= 0 ? prevIdx : 0);
    m_stationFilter->blockSignals(false);
    const qlonglong filterId = m_stationFilter->currentData().toLongLong();

    m_table->setRowCount(0);
    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds)
        return;

    const QueryResult piles = ds->query(QStringLiteral("charging_piles"));
    QVector<DataRow> rows;
    rows.reserve(piles.size());
    for (const DataRow &p : piles) {
        if (filterId > 0 && p.value(QStringLiteral("station_id")).toLongLong() != filterId)
            continue;
        rows << p;
    }

    m_table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const DataRow &p = rows.at(i);
        const qlonglong sid = p.value(QStringLiteral("station_id")).toLongLong();
        const auto put = [&](int col, const QVariant &v) {
            auto *item = new QTableWidgetItem(v.toString());
            item->setData(Qt::UserRole, p.value(QStringLiteral("id")).toLongLong());
            item->setTextAlignment(Qt::AlignCenter);
            m_table->setItem(i, col, item);
        };
        put(0, p.value(QStringLiteral("id")));
        put(1, p.value(QStringLiteral("code")));
        put(2, names.value(sid));
        put(3, p.value(QStringLiteral("type")));
        put(4, QString::number(p.value(QStringLiteral("power_kw")).toDouble(), 'f', 1));
        put(5, QString::number(p.value(QStringLiteral("price_per_kwh")).toDouble(), 'f', 2));
        put(6, p.value(QStringLiteral("status")));
        put(7, p.value(QStringLiteral("charge_count")));
        put(8, p.value(QStringLiteral("charge_duration_min")));
    }
}

QHash<QString, QVariant> PileManagePage::editPileDialog(const QHash<QString, QVariant> &initial)
{
    QDialog dlg(this);
    dlg.setWindowTitle(initial.isEmpty() ? QStringLiteral("新增电桩")
                                         : QStringLiteral("修改电桩"));

    auto *stationCombo = new QComboBox(&dlg);
    stationCombo->addItem(QStringLiteral("请选择电站"), QVariant::fromValue(qlonglong(0)));
    const QueryResult stations = AppContext::instance()->dataSource()
                                     ? AppContext::instance()->dataSource()->query(
                                           QStringLiteral("charging_stations"))
                                     : QueryResult();
    const qlonglong curSid = initial.value(QStringLiteral("station_id")).toLongLong();
    int curSidIdx = -1;
    for (const DataRow &st : stations) {
        const qlonglong sid = st.value(QStringLiteral("id")).toLongLong();
        stationCombo->addItem(st.value(QStringLiteral("name")).toString(),
                              QVariant::fromValue(sid));
        if (sid == curSid)
            curSidIdx = stationCombo->count() - 1;
    }
    stationCombo->setCurrentIndex(curSidIdx >= 0 ? curSidIdx : 0);

    auto *codeEdit = new QLineEdit(initial.value(QStringLiteral("code")).toString(), &dlg);

    auto *typeCombo = new QComboBox(&dlg);
    typeCombo->setEditable(true);
    typeCombo->addItems({QStringLiteral("快充"), QStringLiteral("慢充")});
    const QString curType = initial.value(QStringLiteral("type")).toString();
    if (!curType.isEmpty())
        typeCombo->setCurrentText(curType);

    auto *powerSpin = new QDoubleSpinBox(&dlg);
    powerSpin->setRange(0.5, 500.0);
    powerSpin->setDecimals(1);
    powerSpin->setValue(initial.value(QStringLiteral("power_kw")).isValid()
                            ? initial.value(QStringLiteral("power_kw")).toDouble()
                            : 7.0);

    auto *priceSpin = new QDoubleSpinBox(&dlg);
    priceSpin->setRange(0.01, 999.0);
    priceSpin->setDecimals(2);
    priceSpin->setValue(initial.value(QStringLiteral("price_per_kwh")).isValid()
                            ? initial.value(QStringLiteral("price_per_kwh")).toDouble()
                            : 1.0);

    auto *statusCombo = new QComboBox(&dlg);
    statusCombo->addItems({QStringLiteral("空闲"), QStringLiteral("使用中"),
                           QStringLiteral("故障")});
    const QString curStatus = initial.value(QStringLiteral("status")).toString();
    const int statusIdx = statusCombo->findText(curStatus.isEmpty() ? QStringLiteral("空闲")
                                                                   : curStatus);
    statusCombo->setCurrentIndex(statusIdx >= 0 ? statusIdx : 0);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("所属电站 *"), stationCombo);
    form->addRow(QStringLiteral("桩号 *"), codeEdit);
    form->addRow(QStringLiteral("类型"), typeCombo);
    form->addRow(QStringLiteral("功率(kW)"), powerSpin);
    form->addRow(QStringLiteral("单价(元/度)"), priceSpin);
    form->addRow(QStringLiteral("状态"), statusCombo);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btnBox);

    dlg.setLayout(form);
    dlg.setMinimumWidth(360);

    if (dlg.exec() != QDialog::Accepted)
        return {};
    const qlonglong sid = stationCombo->currentData().toLongLong();
    if (sid <= 0 || codeEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("所属电站和桩号不能为空。"));
        return {};
    }

    QHash<QString, QVariant> values;
    values.insert(QStringLiteral("station_id"), sid);
    values.insert(QStringLiteral("code"), codeEdit->text().trimmed());
    values.insert(QStringLiteral("type"), typeCombo->currentText().trimmed());
    values.insert(QStringLiteral("power_kw"), powerSpin->value());
    values.insert(QStringLiteral("price_per_kwh"), priceSpin->value());
    values.insert(QStringLiteral("status"), statusCombo->currentText());
    return values;
}

void PileManagePage::onAdd()
{
    const QHash<QString, QVariant> values = editPileDialog({});
    if (values.isEmpty())
        return;
    if (DataSource *ds = AppContext::instance()->dataSource())
        ds->insertRow(QStringLiteral("charging_piles"), values);
}

void PileManagePage::onEdit()
{
    const int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选中要修改的电桩。"));
        return;
    }
    const QTableWidgetItem *idItem = m_table->item(row, 0);
    const qlonglong id = idItem ? idItem->data(Qt::UserRole).toLongLong() : -1;
    if (id <= 0)
        return;

    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds)
        return;
    const QueryResult rows = ds->query(QStringLiteral("charging_piles"), {},
                                       QStringLiteral("id = ?"), QVariantList{id});
    if (rows.isEmpty())
        return;

    const QHash<QString, QVariant> values = editPileDialog(rows.first());
    if (values.isEmpty())
        return;
    ds->updateRows(QStringLiteral("charging_piles"), values,
                   QStringLiteral("id = ?"), QVariantList{id});
}

void PileManagePage::onRemove()
{
    const int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选中要删除的电桩。"));
        return;
    }
    const QTableWidgetItem *idItem = m_table->item(row, 0);
    const qlonglong id = idItem ? idItem->data(Qt::UserRole).toLongLong() : -1;
    if (id <= 0)
        return;
    const QString code = m_table->item(row, 1) ? m_table->item(row, 1)->text() : QString();

    if (QMessageBox::question(this, QStringLiteral("删除电桩"),
                              QStringLiteral("确定删除充电桩「%1」吗?").arg(code))
        != QMessageBox::Yes)
        return;

    // 有历史订单/正在使用中的桩不可删,由服务端在事务里判定
    DataSource *ds = AppContext::instance()->dataSource();
    if (ds && !ds->removeChargingPile(id)) {
        QMessageBox::warning(this, QStringLiteral("无法删除"),
                             QStringLiteral("该充电桩正在使用或已有订单记录,不能删除。"));
    }
}
