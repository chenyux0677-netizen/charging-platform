#ifndef CHARGESERVICE_H
#define CHARGESERVICE_H

#include "core/DataSource.h"

#include <QtGlobal>

// 充电结算服务(无真实硬件,结算一律按 时长 × 功率 折算):
//   完成一笔"充电中"订单 → 订单标记已完成并写入电量/金额,
//   电桩回"空闲"并累计次数/时长,用户余额扣款。
// 电量 = 功率(kW) × 时长(分钟) / 60;金额 = 电量 × 单价(元/度)。
// 时长默认取"订单开始时间 → 当前时间"的经过秒数(即模拟分钟数,1 秒 = 1 分钟),
// 因此进程重启后也能正确结算。
namespace ChargeService {

// 结算指定订单；时长和费用全部由服务器计算。
bool settleOrder(DataSource *ds, qlonglong orderId);

} // namespace ChargeService

#endif // CHARGESERVICE_H
