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
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({QStringLiteral("ID"),
                                        QStringLiteral("名称"),
                                        QStringLiteral("地址"),
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

    auto *addBtn = new QPushButton(QStringLiteral("新增电站"), this);
    auto *editBtn = new QPushButton(QStringLiteral("修改"), this);
    auto *removeBtn = new QPushButton(QStringLiteral("删除"), this);
    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(addBtn);
    btnRow->addWidget(editBtn);
    btnRow->addWidget(removeBtn);
    btnRow->addStretch(1);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(m_table, 1);
    layout->addLayout(btnRow);

    connect(addBtn, &QPushButton::clicked, this, &StationManagePage::onAdd);
    connect(editBtn, &QPushButton::clicked, this, &StationManagePage::onEdit);
    connect(removeBtn, &QPushButton::clicked, this, &StationManagePage::onRemove);

    if (DataSource *ds = AppContext::instance()->dataSource()) {
        connect(ds, &DataSource::dataChanged, this, [this](const QString &table) {
            if (table == QStringLiteral("charging_stations"))
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
        put(3, QString::number(st.value(QStringLiteral("lat")).toDouble(), 'f', 6));
        put(4, QString::number(st.value(QStringLiteral("lng")).toDouble(), 'f', 6));
    }
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
    connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
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
    if (nameEdit->text().trimmed().isEmpty() || addrEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("名称和地址不能为空。"));
        return {};
    }

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
    if (DataSource *ds = AppContext::instance()->dataSource())
        ds->insertRow(QStringLiteral("charging_stations"), values);
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

    const QHash<QString, QVariant> values = editStationDialog(rows.first());
    if (values.isEmpty())
        return;
    ds->updateRows(QStringLiteral("charging_stations"), values,
                   QStringLiteral("id = ?"), QVariantList{id});
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

    if (QMessageBox::question(this, QStringLiteral("删除电站"),
                              QStringLiteral("确定删除电站「%1」及其全部充电桩吗?")
                                  .arg(name))
        != QMessageBox::Yes)
        return;

    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds)
        return;
    if (!ds->removeChargingStation(id)) {
        QMessageBox::warning(
            this, QStringLiteral("无法删除"),
            QStringLiteral("该电站包含正在使用或已有订单记录的充电桩，不能删除。"));
    }
}
