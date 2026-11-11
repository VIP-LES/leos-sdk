#include "leos/cyphal/subscriber.hpp"

#include "leos/log.h"

namespace leos::cyphal {

const canard_subscription_vtable_t Subscriber::vtable_ = { &Subscriber::onMessage };

Subscriber::Subscriber(Node& node, std::uint16_t subject_id, std::size_t extent,
                       Callback callback, void* context,
                       canard_us_t transfer_id_timeout) noexcept
    : node_(node), subject_id_(subject_id), extent_(extent), callback_(callback),
      context_(context), transfer_id_timeout_(transfer_id_timeout)
{
}

Subscriber::~Subscriber() noexcept
{
    unsubscribe();
}

bool Subscriber::subscribe() noexcept
{
    if (subscribed_)
        return true;
    if (!node_.initialized_ || (subject_id_ > CANARD_SUBJECT_ID_MAX_13b) ||
        (callback_ == nullptr) || (transfer_id_timeout_ < 0))
        return false;

    canard_subscription_t* const registered = canard_subscribe_13b(
        &node_.canard_, &subscription_, subject_id_, extent_,
        transfer_id_timeout_, &vtable_);
    if (registered != &subscription_)
        return false;

    subscription_.user_context = this;
    subscribed_ = true;
    LOG_DEBUG("Cyphal subscribed to subject %u extent=%u",
              (unsigned)subject_id_, (unsigned)extent_);
    return true;
}

void Subscriber::unsubscribe() noexcept
{
    if (subscribed_) {
        canard_unsubscribe(&node_.canard_, &subscription_);
        subscribed_ = false;
        LOG_DEBUG("Cyphal unsubscribed from subject %u", (unsigned)subject_id_);
    }
}

void Subscriber::onMessage(canard_subscription_t* subscription,
                           canard_us_t timestamp, canard_prio_t priority,
                           uint_least8_t source_node_id,
                           uint_least8_t transfer_id,
                           canard_payload_t payload) noexcept
{
    // A multi-frame payload is owned by the application after libcanard
    // invokes this callback. Retain the allocator before calling application
    // code, which is allowed to unsubscribe this subscription.
    const canard_mem_t rx_memory = subscription->owner->mem.rx_payload;
    auto* subscriber = static_cast<Subscriber*>(subscription->user_context);
    const Message message{payload.view, timestamp, priority,
                          static_cast<std::uint8_t>(source_node_id),
                          static_cast<std::uint8_t>(transfer_id)};
    subscriber->callback_(message, subscriber->context_);
    if (payload.origin.data != nullptr)
        rx_memory.vtable->free(rx_memory, payload.origin.size, payload.origin.data);
}

} // namespace leos::cyphal
