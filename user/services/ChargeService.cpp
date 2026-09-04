#include "ChargeService.h"

namespace ChargeService {

bool stopOrder(DataSource *ds, qlonglong orderId)
{
    return ds && orderId > 0 && ds->stopCharge(orderId);
}

bool settleOrder(DataSource *ds, qlonglong orderId)
{
    return ds && orderId > 0 && ds->settleCharge(orderId);
}

} // namespace ChargeService
