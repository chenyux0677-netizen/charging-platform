#include "OrderPage.h"

#include "common/AppContext.h"
#include "user/services/ChargeService.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
QString displayTime(const QVariant &value)
{
    const QString raw = value.toString();
    const QDateTime time = QDateTime::fromString(raw, Qt::ISODate);
    return time.isValid() ? time.toString(QStringLiteral("yyyy-MM-dd hh:mm")) : raw;
}

QWidget *makeOrderCard(const DataRow &order, const QString &pileCode)
{
    auto *card = new QWidget;
    card->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto *number = new QLabel(
        QStringLiteral("订单 #%1").arg(order.value(QStringLiteral("id")).toLongLong()), card);
    number->setObjectName(QStringLiteral("orderNumberLabel"));

    const QString status = order.value(QStringLiteral("status")).toString();
    auto *statusLabel = new QLabel(status, card);
    if (status == QStringLiteral("充电中"))
        statusLabel->setObjectName(QStringLiteral("orderStatusCharging"));
    else if (status == QStringLiteral("待支付"))
        statusLabel->setObjectName(QStringLiteral("orderStatusPending"));
    else
        statusLabel->setObjectName(QStringLiteral("orderStatusCompleted"));

    auto *header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    header->addWidget(number);
    header->addStretch(1);
    header->addWidget(statusLabel);

    auto *meta = new QLabel(
        QStringLiteral("充电桩 %1  ·  %2")
            .arg(pileCode.isEmpty() ? QStringLiteral("--") : pileCode)
            .arg(displayTime(order.value(QStringLiteral("start_time")))), card);
    meta->setObjectName(QStringLiteral("orderMetaLabel"));

    auto *usage = new QLabel(
        QStringLiteral("%1 分钟  ·  %2 kWh")
            .arg(order.value(QStringLiteral("duration_min")).toLongLong())
            .arg(order.value(QStringLiteral("energy_kwh")).toDouble(), 0, 'f', 2), card);
    usage->setObjectName(QStringLiteral("orderUsageLabel"));

    auto *amount = new QLabel(
        QStringLiteral("¥ %1").arg(order.value(QStringLiteral("amount")).toDouble(), 0, 'f', 2),
        card);
    amount->setObjectName(QStringLiteral("orderCardAmountLabel"));

    auto *summary = new QHBoxLayout;
    summary->setContentsMargins(0, 0, 0, 0);
    summary->addWidget(usage);
    summary->addStretch(1);
    summary->addWidget(amount);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(5);
    layout->addLayout(header);
    layout->addWidget(meta);
    layout->addLayout(summary);
    return card;
}
} // namespace

OrderPage::OrderPage(QWidget *parent)
    : QWidget(parent)
{
    auto *title = new QLabel(QStringLiteral("我的订单"), this);
    title->setObjectName(QStringLiteral("pageTitleLabel"));
    title->setAlignment(Qt::AlignCenter);

    m_orderList = new QListWidget(this);
    m_orderList->setObjectName(QStringLiteral("orderList"));

    m_settleBtn = new QPushButton(QStringLiteral("支付选中订单"), this);
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

    QHash<qlonglong, QString> pileCodes;
    const QueryResult piles = ds->query(QStringLiteral("charging_piles"));
    for (const DataRow &pile : piles) {
        pileCodes.insert(pile.value(QStringLiteral("id")).toLongLong(),
                         pile.value(QStringLiteral("code")).toString());
    }

    // 最近创建的订单排在最前面。
    for (auto it = orders.crbegin(); it != orders.crend(); ++it) {
        const DataRow &o = *it;
        const QString status = o.value(QStringLiteral("status")).toString();
        auto *item = new QListWidgetItem;
        item->setData(Qt::UserRole, o.value(QStringLiteral("id")).toLongLong());
        item->setData(Qt::UserRole + 1, status);
        item->setSizeHint(QSize(0, 94));
        m_orderList->addItem(item);
        m_orderList->setItemWidget(
            item, makeOrderCard(o, pileCodes.value(o.value(QStringLiteral("pile_id")).toLongLong())));
    }
    onCurrentRowChanged();
}

void OrderPage::onCurrentRowChanged()
{
    QListWidgetItem *item = m_orderList->currentItem();
    const QString status = item ? item->data(Qt::UserRole + 1).toString() : QString();
    m_settleBtn->setEnabled(status == QStringLiteral("充电中")
                            || status == QStringLiteral("待支付"));
    m_settleBtn->setText(status == QStringLiteral("充电中")
                             ? QStringLiteral("结束充电并支付")
                             : QStringLiteral("支付选中订单"));
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
    const QString status = item->data(Qt::UserRole + 1).toString();
    const bool stopped = status != QStringLiteral("充电中")
                         || ChargeService::stopOrder(ds, id);
    const bool ok = stopped && ChargeService::settleOrder(ds, id);
    if (ok)
        QMessageBox::information(this, QStringLiteral("结算成功"),
                                 QStringLiteral("订单已结算。"));
    else
        QMessageBox::warning(this, QStringLiteral("支付失败"),
                             stopped
                                 ? QStringLiteral("余额不足，请充值后再次支付。")
                                 : QStringLiteral("停止充电失败，请稍后重试。"));
    refresh();
}
