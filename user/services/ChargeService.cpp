#include "ChargeService.h"

#include <QDebug>

namespace ChargeService {

bool settleOrder(DataSource *ds, qlonglong orderId)
{
    // 计算与多表写入都在服务器端 settleCharge 的单事务里完成,
    // 客户端不重复实现"读余额→算账→逐条直写"的旧逻辑。
    return ds && orderId > 0 && ds->settleCharge(orderId);
}

} // namespace ChargeService
