#include "ChargingPage.h"

#include "common/AppContext.h"
#include "user/services/ChargeService.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

ChargingPage::ChargingPage(QWidget *parent)
    : QWidget(parent)
{
    auto *title = new QLabel(QStringLiteral("充电"), this);
    title->setObjectName(QStringLiteral("pageTitleLabel"));
    title->setAlignment(Qt::AlignCenter);

    auto *pileCard = new QWidget(this);
    pileCard->setObjectName(QStringLiteral("chargePileCard"));

    m_stationLabel = new QLabel(pileCard);
    m_stationLabel->setObjectName(QStringLiteral("chargeStationLabel"));
    m_stationLabel->setWordWrap(true);

    m_pileLabel = new QLabel(pileCard);
    m_pileLabel->setObjectName(QStringLiteral("pileLabel"));
    m_pileLabel->setWordWrap(true);

    m_priceLabel = new QLabel(pileCard);
    m_priceLabel->setObjectName(QStringLiteral("priceLabel"));
    m_priceLabel->setAlignment(Qt::AlignCenter);

    auto *pileSummary = new QHBoxLayout;
    pileSummary->setContentsMargins(0, 0, 0, 0);
    pileSummary->addWidget(m_pileLabel, 1);
    pileSummary->addWidget(m_priceLabel);

    auto *pileCardLayout = new QVBoxLayout(pileCard);
    pileCardLayout->setContentsMargins(14, 12, 14, 12);
    pileCardLayout->setSpacing(8);
    pileCardLayout->addWidget(m_stationLabel);
    pileCardLayout->addLayout(pileSummary);

    auto *progressCard = new QWidget(this);
    progressCard->setObjectName(QStringLiteral("chargeProgressCard"));

    auto *progressCaption = new QLabel(QStringLiteral("本次充电"), progressCard);
    progressCaption->setObjectName(QStringLiteral("chargeProgressCaption"));

    m_energyLabel = new QLabel(QStringLiteral("已充电量:0.00 kWh"), this);
    m_energyLabel->setObjectName(QStringLiteral("energyLabel"));
    m_energyLabel->setAlignment(Qt::AlignCenter);

    m_amountLabel = new QLabel(QStringLiteral("费用:0.00 元"), this);
    m_amountLabel->setObjectName(QStringLiteral("amountLabel"));
    m_amountLabel->setAlignment(Qt::AlignCenter);

    m_timeLabel = new QLabel(QStringLiteral("时长:0 分钟"), this);
    m_timeLabel->setObjectName(QStringLiteral("timeLabel"));
    m_timeLabel->setAlignment(Qt::AlignCenter);

    auto *progressValues = new QHBoxLayout;
    progressValues->setContentsMargins(0, 0, 0, 0);
    progressValues->addWidget(m_energyLabel, 1);
    progressValues->addWidget(m_amountLabel, 1);

    auto *progressLayout = new QVBoxLayout(progressCard);
    progressLayout->setContentsMargins(14, 12, 14, 12);
    progressLayout->setSpacing(8);
    progressLayout->addWidget(progressCaption);
    progressLayout->addWidget(m_timeLabel);
    progressLayout->addLayout(progressValues);

    m_startChargeBtn = new QPushButton(QStringLiteral("开始充电"), this);
    m_startChargeBtn->setObjectName(QStringLiteral("startChargeBtn"));
    m_startChargeBtn->setEnabled(false);

    m_stopChargeBtn = new QPushButton(QStringLiteral("结束并结算"), this);
    m_stopChargeBtn->setObjectName(QStringLiteral("stopChargeBtn"));
    m_stopChargeBtn->setEnabled(false);

    m_timer = new QTimer(this);
    m_timer->setInterval(1000); // 模拟:1 秒 = 1 分钟
    connect(m_timer, &QTimer::timeout, this, &ChargingPage::onTick);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addSpacing(12);
    layout->addWidget(pileCard);
    layout->addWidget(progressCard);
    layout->addSpacing(20);
    layout->addWidget(m_startChargeBtn);
    layout->addWidget(m_stopChargeBtn);
    layout->addStretch(1);

    connect(m_startChargeBtn, &QPushButton::clicked, this, &ChargingPage::startCharging);
    connect(m_stopChargeBtn, &QPushButton::clicked, this, &ChargingPage::stopCharging);
    resetPage();
}

void ChargingPage::resetPage()
{
    m_timer->stop();
    m_pile.clear();
    m_power = 0.0;
    m_price = 0.0;
    m_orderId = 0;
    m_minutes = 0;
    m_charging = false;
    m_progressBusy = false;

    m_stationLabel->setText(QStringLiteral("请先从电站页面选择充电桩"));
    m_pileLabel->setText(QStringLiteral("尚未选择充电桩"));
    m_priceLabel->setText(QStringLiteral("单价:--"));
    m_energyLabel->setText(QStringLiteral("已充电量:0.00 kWh"));
    m_amountLabel->setText(QStringLiteral("费用:0.00 元"));
    m_timeLabel->setText(QStringLiteral("时长:0 分钟"));
    m_startChargeBtn->setEnabled(false);
    m_stopChargeBtn->setEnabled(false);
}

void ChargingPage::setPile(const DataRow &pile, const DataRow &station)
{
    m_pile = pile;
    m_power = pile.value(QStringLiteral("power_kw")).toDouble();
    m_price = pile.value(QStringLiteral("price_per_kwh")).toDouble();
    m_stationLabel->setText(station.value(QStringLiteral("name")).toString());
    m_pileLabel->setText(QStringLiteral("%1  ·  %2  ·  %3 kW")
                             .arg(pile.value(QStringLiteral("code")).toString())
                             .arg(pile.value(QStringLiteral("type")).toString())
                             .arg(m_power, 0, 'f', 0));
    m_priceLabel->setText(QStringLiteral("¥ %1 / 度").arg(m_price, 0, 'f', 2));
    m_energyLabel->setText(QStringLiteral("已充电量:0.00 kWh"));
    m_amountLabel->setText(QStringLiteral("费用:0.00 元"));
    m_timeLabel->setText(QStringLiteral("时长:0 分钟"));
}

bool ChargingPage::findUnfinishedOrder() const
{
    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds)
        return false;
    const qlonglong userId =
        AppContext::instance()->currentUser().value(QStringLiteral("id")).toLongLong();
    const QueryResult orders = ds->query(QStringLiteral("orders"), {},
                                         QStringLiteral("user_id = ?"),
                                         QVariantList{userId});
    for (const DataRow &order : orders) {
        const QString status = order.value(QStringLiteral("status")).toString();
        if (status == QStringLiteral("充电中") || status == QStringLiteral("待支付"))
            return true;
    }
    return false;
}

void ChargingPage::onPageEntered()
{
    if (m_charging)
        return; // 正在充电,不打断
    if (findUnfinishedOrder()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("您有未完成的充电订单,请先到订单页结算。"));
        m_startChargeBtn->setEnabled(false);
        emit goToOrders();
        return;
    }
    m_startChargeBtn->setEnabled(!m_pile.isEmpty());
}

void ChargingPage::startCharging()
{
    if (m_charging || m_pile.isEmpty())
        return;
    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds)
        return;
    // 再保险:仍存在未完成订单则不让开新单
    if (findUnfinishedOrder()) {
        m_startChargeBtn->setEnabled(false);
        emit goToOrders();
        return;
    }

    const qlonglong userId =
        AppContext::instance()->currentUser().value(QStringLiteral("id")).toLongLong();
    const qlonglong orderId = ds->startCharge(
        userId, m_pile.value(QStringLiteral("id")).toLongLong());
    if (orderId <= 0) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("开始充电失败，该充电桩可能已被占用，"
                                            "或当前账号有尚未完成的订单。"));
        return;
    }

    m_orderId = orderId;
    m_charging = true;
    m_minutes = 0;
    m_energyLabel->setText(QStringLiteral("已充电量:0.00 kWh"));
    m_amountLabel->setText(QStringLiteral("费用:0.00 元"));
    m_timeLabel->setText(QStringLiteral("时长:0 分钟"));
    m_startChargeBtn->setEnabled(false);
    m_stopChargeBtn->setEnabled(true);
    m_timer->start();
}

void ChargingPage::onTick()
{
    ++m_minutes;
    const double energy = m_power * m_minutes / 60.0;
    const double amount = energy * m_price;
    m_energyLabel->setText(QStringLiteral("已充电量:%1 kWh").arg(energy, 0, 'f', 2));
    m_amountLabel->setText(QStringLiteral("费用:%1 元").arg(amount, 0, 'f', 2));
    m_timeLabel->setText(QStringLiteral("时长:%1 分钟").arg(m_minutes));
    reportChargingProgress();
}

void ChargingPage::reportChargingProgress()
{
    if (!m_charging || m_orderId <= 0 || m_progressBusy)
        return;
    DataSource *ds = AppContext::instance()->dataSource();
    if (!ds)
        return;
    m_progressBusy = true;
    ds->updateChargingProgress(m_orderId);
    m_progressBusy = false;
}

void ChargingPage::stopCharging()
{
    if (!m_charging)
        return;
    m_timer->stop();
    DataSource *ds = AppContext::instance()->dataSource();
    const bool stopped = ds && ChargeService::stopOrder(ds, m_orderId);
    if (!stopped) {
        m_timer->start();
        QMessageBox::warning(this, QStringLiteral("停止失败"),
                             QStringLiteral("停止充电失败，订单仍在充电，请稍后重试。"));
        return;
    }

    const bool paid = ChargeService::settleOrder(ds, m_orderId);
    resetPage();

    if (paid)
        QMessageBox::information(this, QStringLiteral("结算完成"),
                                 QStringLiteral("充电已结束并完成扣款，明细请在订单页查看。"));
    else if (stopped)
        QMessageBox::warning(this, QStringLiteral("等待支付"),
                             QStringLiteral("充电已结束，但余额不足；费用已冻结，"
                                            "请充值后到订单页支付。"));
    emit goToOrders();
}
