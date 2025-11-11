#include "leos/cyphal/transport.h"
#include "leos/cyphal.h"
#include "leos/log.h"

void leos_cyphal_subscription_dispatch(struct CanardRxTransfer* transfer, struct CanardRxSubscription *subscription) {
    if (!transfer || !subscription) return;

    leos_cyphal_subscription_t* s = (leos_cyphal_subscription_t*) subscription->user_reference;
    if (!s) return;

    LOG_TRACE("Received a completed transfer to port %d", transfer->metadata.port_id);

    s->callback(transfer, s->user_ref);
}