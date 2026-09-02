#ifndef CHARGESERVICE_H
#define CHARGESERVICE_H

#include "core/DataSource.h"

#include <QtGlobal>

// 充电结算服务 —— 现在是给 DataSource 业务接口的薄壳。
// 真实结算(取桩功率/单价、按时长算电量金额、扣余额、释放电桩)已由服务器端
// LocalDataSource::settleCharge 在单事务内完成,客户端只需要转发请求;
// 见 ChargeService.cpp 与 LocalDataSource.cpp 中 settleCharge 的实现。
namespace ChargeService {

// 结算指定订单;成功返回 true。
// 订单不存在、非"充电中"或余额不足时返回 false(余额不足时订单保持"充电中",
// 充值后可重新结算)。时长/金额由服务器按"开始时间 → 当前时间"统一计算。
bool settleOrder(DataSource *ds, qlonglong orderId);

} // namespace ChargeService

#endif // CHARGESERVICE_H
