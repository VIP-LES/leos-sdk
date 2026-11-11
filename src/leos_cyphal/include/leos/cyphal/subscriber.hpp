#pragma once

#include "leos/cyphal/node.hpp"

namespace leos::cyphal {

// The payload view is valid only while the subscriber callback is running.
// Copy it if the application needs to retain it.
struct Message {
    canard_bytes_t payload;
    canard_us_t timestamp;
    canard_prio_t priority;
    std::uint8_t source_node_id;
    std::uint8_t transfer_id;
};

// Keep a Subscriber at a stable address while subscribed. The Node must
// outlive it. One subscriber per subject is supported by libcanard.
class Subscriber final {
public:
    using Callback = void (*)(const Message& message, void* context);

    Subscriber(Node& node, std::uint16_t subject_id, std::size_t extent,
               Callback callback, void* context = nullptr,
               canard_us_t transfer_id_timeout = CANARD_DEFAULT_TRANSFER_ID_TIMEOUT_us) noexcept;
    ~Subscriber() noexcept;

    Subscriber(const Subscriber&) = delete;
    Subscriber& operator=(const Subscriber&) = delete;
    Subscriber(Subscriber&&) = delete;
    Subscriber& operator=(Subscriber&&) = delete;

    // Register after Node::init(). Returns false for invalid arguments or an
    // existing subscription to the same subject. Filters update on next poll.
    bool subscribe() noexcept;
    void unsubscribe() noexcept;

private:
    static void onMessage(canard_subscription_t* subscription,
                          canard_us_t timestamp, canard_prio_t priority,
                          uint_least8_t source_node_id,
                          uint_least8_t transfer_id,
                          canard_payload_t payload) noexcept;

    static const canard_subscription_vtable_t vtable_;

    Node& node_;
    std::uint16_t subject_id_;
    std::size_t extent_;
    Callback callback_;
    void* context_;
    canard_us_t transfer_id_timeout_;
    canard_subscription_t subscription_{};
    bool subscribed_ = false;
};

} // namespace leos::cyphal
