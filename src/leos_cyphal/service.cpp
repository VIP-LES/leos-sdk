#include "leos/cyphal/service.hpp"

#include "leos/log.h"

#include <cstring>

namespace leos::cyphal {

const canard_subscription_vtable_t Service::vtable_ = { &Service::onRequest };

Service::Service(Node& node, std::uint16_t service_id, std::size_t request_extent,
                 Callback callback, void* context,
                 canard_us_t transfer_id_timeout) noexcept
    : node_(node), service_id_(service_id), request_extent_(request_extent),
      callback_(callback), context_(context), transfer_id_timeout_(transfer_id_timeout)
{}

Service::~Service() noexcept {
    stop();
}

bool Service::serve() noexcept {
    if (serving_)
        return true;
    if (!node_.initialized_ || (service_id_ > CANARD_SERVICE_ID_MAX) ||
        (callback_ == nullptr) || (transfer_id_timeout_ < 0))
        return false;

    auto* const registered = canard_subscribe_request(
        &node_.canard_, &subscription_, service_id_, request_extent_,
        transfer_id_timeout_, &vtable_);
    if (registered != &subscription_)
        return false;

    subscription_.user_context = this;
    serving_ = true;
    LOG_DEBUG("Cyphal serving service %u extent=%u",
              (unsigned)service_id_, (unsigned)request_extent_);
    return true;
}

void Service::stop() noexcept {
    if (serving_) {
        canard_unsubscribe(&node_.canard_, &subscription_);
        serving_ = false;
        pending_head_ = 0;
        pending_count_ = 0;
        LOG_DEBUG("Cyphal stopped service %u", (unsigned)service_id_);
    }
}

bool Service::reply(const Request& request, const void* data, std::size_t size,
                    std::uint32_t timeout_us) noexcept
{
    if (!serving_ || (pending_count_ >= queue_capacity) ||
        (size > max_response_size) || ((size != 0U) && (data == nullptr)) ||
        (timeout_us == 0U))
        return false;

    auto& pending = pending_[(pending_head_ + pending_count_) % queue_capacity];
    pending.deadline = request.timestamp + timeout_us;
    pending.priority = request.priority;
    pending.client_node_id = request.client_node_id;
    pending.transfer_id = request.transfer_id;
    pending.size = size;
    if (size != 0U)
        std::memcpy(pending.data, data, size);
    pending_count_++;
    return true;
}

void Service::poll() noexcept {
    if (!serving_)
        return;

    while (pending_count_ != 0U) {
        const auto& pending = pending_[pending_head_];
        if (Node::now(&node_.canard_) >= pending.deadline) {
            LOG_WARNING("Cyphal service %u response expired", (unsigned)service_id_);
        } else {
            const canard_bytes_chain_t payload{{pending.size, pending.data}, nullptr};
            if (!canard_respond(&node_.canard_, pending.deadline, pending.priority,
                                service_id_, pending.client_node_id,
                                pending.transfer_id, payload, nullptr))
                return;
            LOG_TRACE("Cyphal service %u replied to node %u tid=%u size=%u",
                      (unsigned)service_id_, (unsigned)pending.client_node_id,
                      (unsigned)pending.transfer_id, (unsigned)pending.size);
        }
        pending_head_ = (pending_head_ + 1U) % queue_capacity;
        pending_count_--;
    }
}

void Service::onRequest(canard_subscription_t* subscription,
                        canard_us_t timestamp, canard_prio_t priority,
                        uint_least8_t client_node_id,
                        uint_least8_t transfer_id,
                        canard_payload_t payload) noexcept
{
    // Capture the allocator before the callback, which may stop the service.
    const canard_mem_t rx_memory = subscription->owner->mem.rx_payload;
    auto* const service = static_cast<Service*>(subscription->user_context);
    if (service->pending_count_ < queue_capacity) {
        const Request request{payload.view, timestamp, priority,
                              static_cast<std::uint8_t>(client_node_id),
                              static_cast<std::uint8_t>(transfer_id)};
        service->callback_(*service, request, service->context_);
    } else {
        LOG_WARNING("Cyphal service %u response queue full", (unsigned)service->service_id_);
    }
    if (payload.origin.data != nullptr)
        rx_memory.vtable->free(rx_memory, payload.origin.size, payload.origin.data);
}

} // namespace leos::cyphal
