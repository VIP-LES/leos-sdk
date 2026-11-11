#pragma once

#include "canard.h"
#include "leos/mcp251xfd.h"
#include "o1heap.h"

#include <cstddef>
#include <cstdint>

// The MCP251XFD family provides 32 hardware acceptance filters.
#define LEOS_CYPHAL_MCP_FILTER_COUNT 32U

namespace leos::cyphal {

class Publisher;
class Subscriber;

// Cyphal node. Application publishers and subscribers use the same transport.
class Node final {
public:
    enum class Health : std::uint8_t {
        Nominal = 0,
        Advisory = 1,
        Caution = 2,
        Warning = 3,
    };

    enum class Mode : std::uint8_t {
        Operational = 0,
        Initialization = 1,
        Maintenance = 2,
        SoftwareUpdate = 3,
    };

    explicit Node(const leos_mcp251xfd_hw_t& hardware) noexcept;

    // Prevent copying/cloning of the Node object
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(Node&&) = delete;

    // Initialize the MCP and libcanard together. The caller owns the FIFO/filter
    // arrays in config. By default the node installs the Pico GPIO callback;
    // call useExternalIrq() before init() when the application owns it.
    // Libcanard chooses the node ID unless one is passed explicitly.
    eERRORRESULT init(const leos_mcp251xfd_config_t& config,
                      std::uint8_t node_id = CANARD_NODE_ID_ANONYMOUS) noexcept;

    // Opt out of the default callback before init(). The application must
    // register its handler on the same core before init() and forward events
    // to dispatchIrq(). Returns false if the node is already initialized.
    bool useExternalIrq() noexcept;

    // Call from the application's shared Pico GPIO callback. Returns true when
    // this node's MCP IRQ pin had a falling-edge event.
    bool dispatchIrq(uint gpio, std::uint32_t event_mask) noexcept;

    // Latch an MCP interrupt when the caller has already matched its GPIO event.
    void latchCanIrq() noexcept;

    // Run regularly from the main loop, including when no IRQ is pending.
    // Services the MCP, ingests up to 32 received frames, and polls libcanard
    // for filter updates, TX retries, and deadline maintenance.
    void poll() noexcept;

    // Heartbeat is published automatically once per second from poll().
    // Invalid enum values are rejected; uptime is read-only and saturates at
    // UINT32_MAX seconds as required by uavcan.node.Heartbeat.1.0.
    bool setHealth(Health value) noexcept;
    Health health() const noexcept { return health_; }
    bool setMode(Mode value) noexcept;
    Mode mode() const noexcept { return mode_; }
    void setVendorStatusCode(std::uint8_t value) noexcept;
    std::uint8_t vendorStatusCode() const noexcept { return vendor_status_code_; }
    std::uint32_t uptimeSeconds() const noexcept;

    leos_mcp251xfd_t& mcp() noexcept { return mcp_; }
    canard_t& canard() noexcept { return canard_; }

private:
    friend class Publisher;
    friend class Subscriber;

    static constexpr std::size_t heap_size_ = 32U * 1024U;
    static constexpr std::size_t tx_queue_capacity_ = 32U;
    static constexpr std::size_t max_rx_frames_per_poll_ = 32U;
    static constexpr std::uint16_t heartbeat_subject_id_ = 7509U;
    static constexpr canard_us_t heartbeat_period_us_ = 1000000U;

    bool publishHeartbeat(canard_us_t timestamp) noexcept;
    static void free(canard_mem_t memory, std::size_t size, void* ptr) noexcept;
    static void* allocate(canard_mem_t memory, std::size_t size) noexcept;
    static void picoIrqCallback(uint gpio, std::uint32_t event_mask) noexcept;
    static canard_us_t now(const canard_t* self) noexcept;
    static bool transmit(canard_t* self, void* user_context, canard_us_t deadline,
                         uint_least8_t iface_index, bool fd, std::uint32_t can_id,
                         canard_bytes_t data) noexcept;
    static bool configure_filters(canard_t* self, std::size_t filter_count,
                                  const canard_filter_t* filters) noexcept;

    static const canard_vtable_t vtable_;
    static const canard_mem_vtable_t memory_vtable_;
    static Node* irq_node_;

    leos_mcp251xfd_t mcp_{};
    canard_t canard_{};
    alignas(O1HEAP_ALIGNMENT) std::uint8_t heap_arena_[heap_size_]{};
    O1HeapInstance* heap_ = nullptr;
    bool initialized_ = false;
    bool external_irq_ = false;
    Health health_ = Health::Nominal;
    Mode mode_ = Mode::Initialization;
    std::uint8_t vendor_status_code_ = 0;
    std::uint8_t heartbeat_tid_ = 0;
    canard_us_t next_heartbeat_us_ = 0;
    bool heartbeat_enqueue_failed_ = false;
    std::uint64_t reported_tx_expiration_ = 0;
    std::uint64_t reported_tx_sacrifice_ = 0;
    canard_us_t next_tx_loss_log_us_ = 0;
};

} // namespace leos::cyphal