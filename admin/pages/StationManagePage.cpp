#include "StationManagePage.h"

#include "common/AppContext.h"
#include "common/WidgetUtil.h"
#include "user/services/MapService.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

StationManagePage::StationManagePage(QWidget *parent)
    : QWidget(parent)
{
    auto *title = new QLabel(QStringLiteral("充电站管理"), this);
    title->setObjectName(QStringLiteral("pageTitleLabel"));

    m_table = new QTableWidget(this);
    m_table->setObjectName(QStringLiteral("stationManageTable"));
    m_table->setColumnCount(8);
    m_table->setHorizontalHeaderLabels({QStringLiteral("ID"),
                                        QStringLiteral("名称"),
                                        QStringLiteral("地址"),
                                        QStringLiteral("电桩总数"),
                                        QStringLiteral("正常率（非故障）"),
                                        QStringLiteral("使用率（充电中）"),
                                        QStringLiteral("纬度"),
                                        QStringLiteral("经度")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    QHeaderView *header = m_table->horizontalHeader();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(2, QHeaderView::Stretch);
    header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(7, QHeaderView::ResizeToContents);

    auto *addBtn = new QPushButton(QStringLiteral("新增电站"), this);
    addBtn->setObjectName(QStringLiteral("adminAddButton"));
    auto *editBtn = new QPushButton(QStringLiteral("修改"), this);
    editBtn->setObjectName(QStringLiteral("adminEditButton"));
    auto *removeBtn = new QPushButton(QStringLiteral("删除"), this);
    removeBtn->setObjectName(QStringLiteral("adminDeleteButton"));
    auto *detailsBtn = new QPushButton(QStringLiteral("查看电桩"), this);
    detailsBtn->setObjectName(QStringLiteral("adminDetailsButton"));
    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(addBtn);
    btnRow->addWidget(editBtn);
    btnRow->addWidget(removeBtn);
    btnRow->addWidget(detailsBtn);
    btnRow->addStretch(1);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(m_table, 1);
    layout->addLayout(btnRow);

    connect(addBtn, &QPushButton::clicked, this, &StationManagePage::onAdd);
    connect(editBtn, &QPushButton::clicked, this, &StationManagePage::onEdit);
    connect(removeBtn, &QPushButton::clicked, this, &StationManagePage::onRemove);
    connect(detailsBtn, &QPushButton::clicked, this, &StationManagePage::onDetails);
    connect(m_table, &QTableWidget::cellDoubleClicked, this,
            [this](int, int) { onDetails(); });

    if (DataSource *ds = AppContext::instance()->dataSource()) {
        connect(ds, &DataSource::dataChanged, this, [this](const QString &table) {
            if (table == QStringLiteral("charging_stations")
                || table == QStringLiteral("charging_piles"))
                refresh();
        });
    }

    refresh();
}

void StationManagePage::refresh()
{
    m_table->setRowCount(0);
    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds)
        return;

    const QueryResult stations = ds->query(QStringLiteral("charging_stations"));
    const QueryResult piles = ds->query(QStringLiteral("charging_piles"));
    QHash<qlonglong, int> totalByStation;
    QHash<qlonglong, int> normalByStation;
    QHash<qlonglong, int> busyByStation;
    for (const DataRow &pile : piles) {
        const qlonglong stationId = pile.value(QStringLiteral("station_id")).toLongLong();
        ++totalByStation[stationId];
        const QString status = pile.value(QStringLiteral("status")).toString();
        if (status == QStringLiteral("空闲") || status == QStringLiteral("使用中"))
            ++normalByStation[stationId];
        if (status == QStringLiteral("使用中"))
            ++busyByStation[stationId];
    }

    m_table->setRowCount(stations.size());
    for (int i = 0; i < stations.size(); ++i) {
        const DataRow &st = stations.at(i);
        const auto put = [&](int col, const QVariant &v) {
            auto *item = new QTableWidgetItem(v.toString());
            item->setData(Qt::UserRole, st.value(QStringLiteral("id")).toLongLong());
            item->setTextAlignment(Qt::AlignCenter);
            m_table->setItem(i, col, item);
        };
        put(0, st.value(QStringLiteral("id")));
        put(1, st.value(QStringLiteral("name")));
        put(2, st.value(QStringLiteral("address")));
        const qlonglong stationId = st.value(QStringLiteral("id")).toLongLong();
        const int total = totalByStation.value(stationId);
        const int normal = normalByStation.value(stationId);
        const int busy = busyByStation.value(stationId);
        put(3, QStringLiteral("%1 根").arg(total));
        put(4, total > 0
                   ? QStringLiteral("%1% (%2/%3)")
                         .arg(normal * 100.0 / total, 0, 'f', 1)
                         .arg(normal)
                         .arg(total)
                   : QStringLiteral("—"));
        put(5, total > 0
                   ? QStringLiteral("%1% (%2/%3)")
                         .arg(busy * 100.0 / total, 0, 'f', 1)
                         .arg(busy)
                         .arg(total)
                   : QStringLiteral("—"));
        put(6, QString::number(st.value(QStringLiteral("lat")).toDouble(), 'f', 6));
        put(7, QString::number(st.value(QStringLiteral("lng")).toDouble(), 'f', 6));
    }
}

void StationManagePage::onDetails()
{
    const int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选中要查看的电站。"));
        return;
    }
    const QTableWidgetItem *idItem = m_table->item(row, 0);
    const qlonglong stationId = idItem ? idItem->data(Qt::UserRole).toLongLong() : -1;
    if (stationId <= 0)
        return;

    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds)
        return;
    const QueryResult piles = ds->query(QStringLiteral("charging_piles"), {},
                                        QStringLiteral("station_id = ?"),
                                        QVariantList{stationId});

    QDialog dlg(this);
    const QString stationName = m_table->item(row, 1) ? m_table->item(row, 1)->text()
                                                       : QString();
    dlg.setWindowTitle(QStringLiteral("%1 - 电桩详情").arg(stationName));
    dlg.setMinimumSize(650, 360);

    auto *heading = new QLabel(QStringLiteral("%1（共 %2 根）").arg(stationName).arg(piles.size()),
                               &dlg);
    heading->setObjectName(QStringLiteral("stationPileDetailsTitle"));
    auto *table = new QTableWidget(&dlg);
    table->setObjectName(QStringLiteral("stationPileDetailsTable"));
    table->setColumnCount(5);
    table->setHorizontalHeaderLabels({QStringLiteral("桩号"),
                                      QStringLiteral("类型"),
                                      QStringLiteral("功率(kW)"),
                                      QStringLiteral("状态"),
                                      QStringLiteral("单价(元/kWh)")});
    table->setRowCount(piles.size());
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    for (int i = 0; i < piles.size(); ++i) {
        const DataRow &pile = piles.at(i);
        const QStringList texts{
            pile.value(QStringLiteral("code")).toString(),
            pile.value(QStringLiteral("type")).toString(),
            QString::number(pile.value(QStringLiteral("power_kw")).toDouble(), 'f', 1),
            pile.value(QStringLiteral("status")).toString(),
            QString::number(pile.value(QStringLiteral("price_per_kwh")).toDouble(), 'f', 2)};
        for (int col = 0; col < texts.size(); ++col) {
            auto *item = new QTableWidgetItem(texts.at(col));
            item->setTextAlignment(Qt::AlignCenter);
            table->setItem(i, col, item);
        }
    }

    auto *closeBox = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(closeBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    auto *layout = new QVBoxLayout(&dlg);
    layout->addWidget(heading);
    layout->addWidget(table, 1);
    layout->addWidget(closeBox);
    dlg.exec();
}

QHash<QString, QVariant> StationManagePage::editStationDialog(const QHash<QString, QVariant> &initial)
{
    QDialog dlg(this);
    dlg.setWindowTitle(initial.isEmpty() ? QStringLiteral("新增电站")
                                         : QStringLiteral("修改电站"));

    auto *searchEdit = new QLineEdit(&dlg);
    searchEdit->setObjectName(QStringLiteral("stationLocationSearchEdit"));
    searchEdit->setPlaceholderText(QStringLiteral("全国搜索地点关键词或地址"));

    auto *suggestionList = new QListWidget(&dlg);
    suggestionList->setObjectName(QStringLiteral("stationLocationSuggestionList"));
    suggestionList->hide();

    auto *nameEdit = new QLineEdit(initial.value(QStringLiteral("name")).toString(), &dlg);
    auto *addrEdit = new QLineEdit(initial.value(QStringLiteral("address")).toString(), &dlg);
    addrEdit->setMinimumWidth(420);

    auto *latSpin = new QDoubleSpinBox(&dlg);
    latSpin->setRange(-90.0, 90.0);
    latSpin->setDecimals(6);
    latSpin->setValue(initial.value(QStringLiteral("lat")).toDouble());

    auto *lngSpin = new QDoubleSpinBox(&dlg);
    lngSpin->setRange(-180.0, 180.0);
    lngSpin->setDecimals(6);
    lngSpin->setValue(initial.value(QStringLiteral("lng")).toDouble());

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("地点搜索"), searchEdit);
    form->addRow(QStringLiteral("名称 *"), nameEdit);
    form->addRow(QStringLiteral("地址 *"), addrEdit);
    form->addRow(QStringLiteral("纬度"), latSpin);
    form->addRow(QStringLiteral("经度"), lngSpin);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(btnBox, &QDialogButtonBox::accepted, &dlg, [&] {
        if (nameEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dlg, QStringLiteral("提示"),
                                 QStringLiteral("电站名称不能为空。"));
            nameEdit->setFocus();
            return;
        }
        if (addrEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dlg, QStringLiteral("提示"),
                                 QStringLiteral("电站地址不能为空。"));
            addrEdit->setFocus();
            return;
        }
        dlg.accept();
    });
    form->addRow(btnBox);

    dlg.setLayout(form);
    dlg.setMinimumWidth(600);

    auto *mapService = new MapService(&dlg);
    auto *suggestionTimer = new QTimer(&dlg);
    suggestionTimer->setSingleShot(true);
    suggestionTimer->setInterval(500);
    if (!mapService->hasApiKey()) {
        searchEdit->setEnabled(false);
        searchEdit->setPlaceholderText(QStringLiteral("未配置地图 Key，可手动填写下方信息"));
    }
    connect(searchEdit, &QLineEdit::textEdited, &dlg,
            [mapService, suggestionTimer](const QString &text) {
        suggestionTimer->stop();
        if (text.trimmed().size() < 2) {
            mapService->suggest(QString());
            return;
        }
        suggestionTimer->start();
    });
    connect(suggestionTimer, &QTimer::timeout, &dlg,
            [mapService, searchEdit] { mapService->suggest(searchEdit->text()); });
    connect(mapService, &MapService::suggestionsSucceeded, &dlg,
            [suggestionList, searchEdit](const QVariantList &suggestions) {
        suggestionList->clear();
        for (const QVariant &value : suggestions) {
            const QVariantMap suggestion = value.toMap();
            const QString title = suggestion.value(QStringLiteral("title")).toString();
            const QString address = suggestion.value(QStringLiteral("address")).toString();
            auto *item = new QListWidgetItem(
                address.isEmpty() ? title : QStringLiteral("%1\n%2").arg(title, address));
            item->setData(Qt::UserRole, suggestion);
            suggestionList->addItem(item);
        }
        WidgetUtil::showSuggestionPopup(suggestionList, searchEdit);
    });
    connect(mapService, &MapService::suggestionsFailed, &dlg,
            [suggestionList, searchEdit](const QString &message) {
        suggestionList->clear();
        auto *item = new QListWidgetItem(message);
        item->setFlags(Qt::NoItemFlags);
        suggestionList->addItem(item);
        WidgetUtil::showSuggestionPopup(suggestionList, searchEdit);
    });
    const auto applySuggestion = [=](QListWidgetItem *item) {
        if (!item || !(item->flags() & Qt::ItemIsEnabled))
            return;
        const QVariantMap suggestion = item->data(Qt::UserRole).toMap();
        const QString title = suggestion.value(QStringLiteral("title")).toString();
        const QString address = suggestion.value(QStringLiteral("address")).toString();
        searchEdit->setText(title);
        if (nameEdit->text().trimmed().isEmpty())
            nameEdit->setText(title);
        addrEdit->setText(address.isEmpty() ? title : address);
        latSpin->setValue(suggestion.value(QStringLiteral("lat")).toDouble());
        lngSpin->setValue(suggestion.value(QStringLiteral("lng")).toDouble());
        suggestionList->hide();
    };
    connect(suggestionList, &QListWidget::itemClicked, &dlg, applySuggestion);
    connect(suggestionList, &QListWidget::itemActivated, &dlg, applySuggestion);

    if (dlg.exec() != QDialog::Accepted)
        return {};

    QHash<QString, QVariant> values;
    values.insert(QStringLiteral("name"), nameEdit->text().trimmed());
    values.insert(QStringLiteral("address"), addrEdit->text().trimmed());
    values.insert(QStringLiteral("lat"), latSpin->value());
    values.insert(QStringLiteral("lng"), lngSpin->value());
    return values;
}

void StationManagePage::onAdd()
{
    const QHash<QString, QVariant> values = editStationDialog({});
    if (values.isEmpty())
        return;
    if (DataSource *ds = AppContext::instance()->dataSource()) {
        if (ds->insertRow(QStringLiteral("charging_stations"), values) <= 0) {
            QMessageBox::warning(this, QStringLiteral("新增失败"),
                                 QStringLiteral("未能新增电站，请稍后重试。"));
        }
    }
    // 数据变更经 dataChanged 广播自动刷新
}

void StationManagePage::onEdit()
{
    const int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选中要修改的电站。"));
        return;
    }
    const QTableWidgetItem *idItem = m_table->item(row, 0);
    const qlonglong id = idItem ? idItem->data(Qt::UserRole).toLongLong() : -1;
    if (id <= 0)
        return;

    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds)
        return;
    const QueryResult rows = ds->query(QStringLiteral("charging_stations"), {},
                                       QStringLiteral("id = ?"), QVariantList{id});
    if (rows.isEmpty())
        return;
    const QueryResult activePiles = ds->query(
        QStringLiteral("charging_piles"), {QStringLiteral("id")},
        QStringLiteral("station_id = ? AND status = '使用中'"), QVariantList{id});
    if (!activePiles.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("无法修改"),
                             QStringLiteral("该电站有电桩正在充电，结束充电后才能修改。"));
        return;
    }

    const QHash<QString, QVariant> values = editStationDialog(rows.first());
    if (values.isEmpty())
        return;
    if (ds->updateRows(QStringLiteral("charging_stations"), values,
                       QStringLiteral("id = ?"), QVariantList{id}) <= 0) {
        QMessageBox::warning(this, QStringLiteral("修改失败"),
                             QStringLiteral("未能修改电站；站内可能有电桩正在充电。"));
    }
}

void StationManagePage::onRemove()
{
    const int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选中要删除的电站。"));
        return;
    }
    const QTableWidgetItem *idItem = m_table->item(row, 0);
    const qlonglong id = idItem ? idItem->data(Qt::UserRole).toLongLong() : -1;
    if (id <= 0)
        return;
    const QString name = m_table->item(row, 1) ? m_table->item(row, 1)->text() : QString();

    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds)
        return;
    const QueryResult activePiles = ds->query(
        QStringLiteral("charging_piles"), {QStringLiteral("id")},
        QStringLiteral("station_id = ? AND status = '使用中'"), QVariantList{id});
    if (!activePiles.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("无法删除"),
                             QStringLiteral("该电站有电桩正在充电，结束充电后才能删除。"));
        return;
    }

    if (QMessageBox::question(this, QStringLiteral("删除电站"),
                              QStringLiteral("确定删除电站「%1」及其全部充电桩吗?")
                                  .arg(name))
        != QMessageBox::Yes)
        return;

    if (!ds->removeChargingStation(id)) {
        QMessageBox::warning(
            this, QStringLiteral("无法删除"),
            QStringLiteral("该电站包含正在使用或已有订单记录的充电桩，不能删除。"));
    }
}
