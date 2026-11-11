#include "leos/cyphal/publisher.hpp"

#include "leos/log.h"

namespace leos::cyphal {

Publisher::Publisher(Node& node, std::uint16_t subject_id,
                     canard_prio_t priority) noexcept
    : node_(node), subject_id_(subject_id), priority_(priority)
{
}

Publisher::Result Publisher::publish(const void* data, std::size_t size,
                                     std::uint32_t timeout_us) noexcept
{
    if (!node_.initialized_)
        return Result::NotInitialized;
    if ((subject_id_ > CANARD_SUBJECT_ID_MAX_13b) ||
        (subject_id_ == Node::heartbeat_subject_id_) ||
        (static_cast<unsigned>(priority_) >= CANARD_PRIO_COUNT) ||
        ((size != 0U) && (data == nullptr)) || (timeout_us == 0U))
        return Result::InvalidArgument;

    const canard_bytes_chain_t payload{{size, data}, nullptr};
    const std::uint64_t previous_capacity = node_.canard_.err.tx_capacity;
    const std::uint64_t previous_oom = node_.canard_.err.oom;
    const bool queued = canard_publish_13b(
        &node_.canard_, Node::now(&node_.canard_) + timeout_us, 1U,
        priority_, subject_id_, transfer_id_, payload, nullptr);
    if (!queued) {
        if (node_.canard_.err.tx_capacity != previous_capacity)
            return Result::QueueFull;
        if (node_.canard_.err.oom != previous_oom)
            return Result::OutOfMemory;
        return Result::Rejected;
    }

    LOG_TRACE("Cyphal subject %u queued tid=%u size=%u",
              (unsigned)subject_id_, (unsigned)transfer_id_, (unsigned)size);
    transfer_id_ = static_cast<std::uint8_t>((transfer_id_ + 1U) & CANARD_TRANSFER_ID_MAX);
    return Result::Queued;
}

} // namespace leos::cyphal
