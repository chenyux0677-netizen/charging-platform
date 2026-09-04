#ifndef CHARGESERVICE_H
#define CHARGESERVICE_H

#include "core/DataSource.h"

#include <QtGlobal>

// 停止时冻结费用并释放电桩；支付时才扣减余额。
// 电量 = 功率(kW) × 时长(分钟) / 60;金额 = 电量 × 单价(元/度)。
// 时长默认取"订单开始时间 → 当前时间"的经过秒数(即模拟分钟数,1 秒 = 1 分钟),
// 因此进程重启后也能按实际经过时间停止并冻结费用。
namespace ChargeService {

bool stopOrder(DataSource *ds, qlonglong orderId);
bool settleOrder(DataSource *ds, qlonglong orderId);

} // namespace ChargeService

#endif // CHARGESERVICE_H
