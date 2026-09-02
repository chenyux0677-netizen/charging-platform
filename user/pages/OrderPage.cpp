#include "OrderPage.h"

#include "common/AppContext.h"
#include "user/services/ChargeService.h"

#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

OrderPage::OrderPage(QWidget *parent)
    : QWidget(parent)
{
    auto *title = new QLabel(QStringLiteral("我的订单"), this);
    title->setObjectName(QStringLiteral("pageTitleLabel"));
    title->setAlignment(Qt::AlignCenter);

    m_orderList = new QListWidget(this);
    m_orderList->setObjectName(QStringLiteral("orderList"));

    m_settleBtn = new QPushButton(QStringLiteral("结算选中订单"), this);
    m_settleBtn->setObjectName(QStringLiteral("settleBtn"));
    m_settleBtn->setEnabled(false);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(m_orderList, 1);
    layout->addWidget(m_settleBtn);

    connect(m_settleBtn, &QPushButton::clicked, this, &OrderPage::onSettleClicked);
    connect(m_orderList, &QListWidget::currentRowChanged,
            this, &OrderPage::onCurrentRowChanged);

    // 订单状态变化(结算完成) → 自动刷新
    if (DataSource *ds = AppContext::instance()->dataSource()) {
        connect(ds, &DataSource::dataChanged, this, [this](const QString &table) {
            if (table == QStringLiteral("orders"))
                refresh();
        });
    }
}

void OrderPage::refresh()
{
    m_orderList->clear();
    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds)
        return;

    const qlonglong userId =
        AppContext::instance()->currentUser().value(QStringLiteral("id")).toLongLong();
    const QueryResult orders = ds->query(QStringLiteral("orders"), {},
                                         QStringLiteral("user_id = ?"),
                                         QVariantList{userId});
    if (orders.isEmpty()) {
        auto *empty = new QListWidgetItem(QStringLiteral("暂无订单"));
        empty->setFlags(Qt::NoItemFlags);
        m_orderList->addItem(empty);
        m_settleBtn->setEnabled(false);
        return;
    }
    for (const DataRow &o : orders) {
        const QString status = o.value(QStringLiteral("status")).toString();
        QString text;
        if (status == QStringLiteral("充电中")) {
            text = QStringLiteral("订单#%1 | 开始 %2 | %3")
                .arg(o.value(QStringLiteral("id")).toLongLong())
                .arg(o.value(QStringLiteral("start_time")).toString())
                .arg(status);
        } else {
            text = QStringLiteral("订单#%1 | 开始 %2 | %3 | %4 kWh | %5 元")
                .arg(o.value(QStringLiteral("id")).toLongLong())
                .arg(o.value(QStringLiteral("start_time")).toString())
                .arg(status)
                .arg(o.value(QStringLiteral("energy_kwh")).toDouble(), 0, 'f', 2)
                .arg(o.value(QStringLiteral("amount")).toDouble(), 0, 'f', 2);
        }
        auto *item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, o.value(QStringLiteral("id")).toLongLong());
        item->setData(Qt::UserRole + 1, status);
        m_orderList->addItem(item);
    }
    onCurrentRowChanged();
}

void OrderPage::onCurrentRowChanged()
{
    QListWidgetItem *item = m_orderList->currentItem();
    m_settleBtn->setEnabled(item
                            && item->data(Qt::UserRole + 1).toString()
                                   == QStringLiteral("充电中"));
}

void OrderPage::onSettleClicked()
{
    QListWidgetItem *item = m_orderList->currentItem();
    if (!item)
        return;
    const qlonglong id = item->data(Qt::UserRole).toLongLong();
    if (id <= 0)
        return;

    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds)
        return;
    const bool ok = ChargeService::settleOrder(ds, id);
    if (ok)
        QMessageBox::information(this, QStringLiteral("结算成功"),
                                 QStringLiteral("订单已结算。"));
    else
        QMessageBox::warning(this, QStringLiteral("结算失败"),
                             QStringLiteral("余额不足、订单状态异常或结算失败。"));
    refresh();
}
