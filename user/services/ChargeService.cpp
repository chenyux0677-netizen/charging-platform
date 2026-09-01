#include "ChargeService.h"

#include <QDateTime>
#include <QDebug>

namespace ChargeService {

bool settleOrder(DataSource *ds, qlonglong orderId, double simulatedMinutes)
{
    if (!ds || orderId <= 0)
        return false;

    // 只结算"充电中"的订单
    const QueryResult orders = ds->query(QStringLiteral("orders"),
                                         {},
                                         QStringLiteral("id = ? AND status = '充电中'"),
                                         QVariantList{orderId});
    if (orders.isEmpty())
        return false;
    const DataRow order = orders.first();

    // 取对应电桩(功率、单价)
    const QueryResult piles = ds->query(QStringLiteral("charging_piles"),
                                        {},
                                        QStringLiteral("id = ?"),
                                        QVariantList{order.value(QStringLiteral("pile_id"))});
    if (piles.isEmpty())
        return false;
    const DataRow pile = piles.first();

    const qlonglong userId = order.value(QStringLiteral("user_id")).toLongLong();
    const double power = pile.value(QStringLiteral("power_kw")).toDouble();
    const double price = pile.value(QStringLiteral("price_per_kwh")).toDouble();

    // 时长(分钟):优先用界面计时的"模拟分钟"(1 秒 = 1 分钟),
    // 保证结算结果与用户端显示一致;
    // 进程重启后恢复结算没有内存计数,退回"开始时间 → 当前时间"的经过秒数
    // (1 真实秒 = 1 模拟分钟),异常时至少按 1 分钟计。
    const QDateTime start = QDateTime::fromString(
        order.value(QStringLiteral("start_time")).toString(), Qt::ISODate);
    const double elapsedSec = start.isValid()
        ? static_cast<double>(start.secsTo(QDateTime::currentDateTime()))
        : 0.0;
    const double minutes = simulatedMinutes > 0.0
        ? simulatedMinutes
        : qMax(1.0, elapsedSec);

    const double energy = power * minutes / 60.0;
    const double amount = energy * price;
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);

    // 1) 订单 → 已完成
    QHash<QString, QVariant> orderUp;
    orderUp.insert(QStringLiteral("end_time"), now);
    orderUp.insert(QStringLiteral("energy_kwh"), energy);
    orderUp.insert(QStringLiteral("amount"), amount);
    orderUp.insert(QStringLiteral("status"), QStringLiteral("已完成"));
    const int o = ds->updateRows(QStringLiteral("orders"), orderUp,
                                 QStringLiteral("id = ?"), QVariantList{orderId});

    // 2) 电桩 → 空闲,累计次数/时长
    QHash<QString, QVariant> pileUp;
    pileUp.insert(QStringLiteral("status"), QStringLiteral("空闲"));
    pileUp.insert(QStringLiteral("charge_count"),
                  pile.value(QStringLiteral("charge_count")).toLongLong() + 1);
    pileUp.insert(QStringLiteral("charge_duration_min"),
                  pile.value(QStringLiteral("charge_duration_min")).toLongLong()
                      + static_cast<qlonglong>(qRound64(minutes)));
    const int p = ds->updateRows(QStringLiteral("charging_piles"), pileUp,
                                 QStringLiteral("id = ?"),
                                 QVariantList{order.value(QStringLiteral("pile_id"))});

    // 3) 用户余额扣款(演示:允许扣成负数)
    const QueryResult users = ds->query(QStringLiteral("users"),
                                        {},
                                        QStringLiteral("id = ?"),
                                        QVariantList{userId});
    if (!users.isEmpty()) {
        QHash<QString, QVariant> u;
        u.insert(QStringLiteral("balance"),
                 users.first().value(QStringLiteral("balance")).toDouble() - amount);
        ds->updateRows(QStringLiteral("users"), u,
                       QStringLiteral("id = ?"), QVariantList{userId});
    }

    if (o != 1 || p != 1) {
        qWarning() << "[ChargeService] 结算失败 orderId =" << orderId
                   << "orders更新=" << o << "piles更新=" << p;
        return false;
    }

    qInfo() << "[ChargeService] 订单" << orderId << "结算完成:"
            << QStringLiteral("%1 分钟, %2 kWh, %3 元")
                   .arg(minutes, 0, 'f', 0)
                   .arg(energy, 0, 'f', 2)
                   .arg(amount, 0, 'f', 2);
    return true;
}

} // namespace ChargeService
