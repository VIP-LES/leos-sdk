#pragma once

#include "leos/cyphal/node.hpp"

namespace leos::cyphal {

// One publisher per subject keeps that subject's transfer IDs in sequence.
// The Node must outlive its publishers. Do not create multiple publishers for
// the same subject on one Node: their independent transfer IDs would conflict.
class Publisher final {
public:
    enum class Result : std::uint8_t {
        Queued,
        NotInitialized,
        InvalidArgument,
        QueueFull,
        OutOfMemory,
        Rejected,
    };

    Publisher(Node& node, std::uint16_t subject_id,
              canard_prio_t priority = canard_prio_nominal) noexcept;

    Publisher(const Publisher&) = delete;
    Publisher& operator=(const Publisher&) = delete;
    Publisher(Publisher&&) = delete;
    Publisher& operator=(Publisher&&) = delete;

    // The caller serializes its DSDL payload. Queued means libcanard accepted
    // the transfer, not that it has reached the CAN bus. Call Node::poll() to
    // transmit it. A failed enqueue leaves the transfer ID unchanged.
    Result publish(const void* data, std::size_t size,
                   std::uint32_t timeout_us = 1000000U) noexcept;

private:
    Node& node_;
    std::uint16_t subject_id_;
    canard_prio_t priority_;
    std::uint8_t transfer_id_ = 0;
};

} // namespace leos::cyphal
