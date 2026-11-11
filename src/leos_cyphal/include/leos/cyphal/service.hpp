#pragma once

#include "leos/cyphal/node.hpp"

namespace leos::cyphal {

// The payload view is valid only during the callback. The remaining fields
// identify the client transfer and may be copied for a later reply.
struct Request {
    canard_bytes_t payload;
    canard_us_t timestamp;
    canard_prio_t priority;
    std::uint8_t client_node_id;
    std::uint8_t transfer_id;
};

// One Service per service-ID on a Node. Keep it at a stable address while
// serving; the Node must outlive it. Call poll() after Node::poll().
class Service final {
public:
    using Callback = void (*)(Service& service, const Request& request, void* context);

    static constexpr std::size_t max_response_size = 64U;
    static constexpr std::size_t queue_capacity = 32U;

    Service(Node& node, std::uint16_t service_id, std::size_t request_extent,
            Callback callback, void* context = nullptr,
            canard_us_t transfer_id_timeout = CANARD_DEFAULT_TRANSFER_ID_TIMEOUT_us) noexcept;
    ~Service() noexcept;

    Service(const Service&) = delete;
    Service& operator=(const Service&) = delete;
    Service(Service&&) = delete;
    Service& operator=(Service&&) = delete;

    // Register after Node::init(). Filters update on the next Node::poll().
    bool serve() noexcept;
    void stop() noexcept;

    // Copy a response for a Request delivered by this Service. Safe in the
    // callback: libcanard is not called until poll(). Returns false if the
    // response is too large, the queue is full, or the data is invalid.
    bool reply(const Request& request, const void* data, std::size_t size,
               std::uint32_t timeout_us = 1000000U) noexcept;

    // Enqueue pending replies after Node::poll() returns. A full libcanard TX
    // queue is retried until the reply deadline, then the reply is discarded.
    void poll() noexcept;

private:
    struct PendingReply {
        canard_us_t deadline;
        canard_prio_t priority;
        std::uint8_t client_node_id;
        std::uint8_t transfer_id;
        std::size_t size;
        std::uint8_t data[max_response_size];
    };

    static void onRequest(canard_subscription_t* subscription,
                          canard_us_t timestamp, canard_prio_t priority,
                          uint_least8_t client_node_id,
                          uint_least8_t transfer_id,
                          canard_payload_t payload) noexcept;

    static const canard_subscription_vtable_t vtable_;

    Node& node_;
    std::uint16_t service_id_;
    std::size_t request_extent_;
    Callback callback_;
    void* context_;
    canard_us_t transfer_id_timeout_;
    canard_subscription_t subscription_{};
    PendingReply pending_[queue_capacity]{};
    std::size_t pending_head_ = 0;
    std::size_t pending_count_ = 0;
    bool serving_ = false;
};

} // namespace leos::cyphal
