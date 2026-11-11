#include "leos/cyphal/node.hpp"

#include "hardware/gpio.h"
#include "leos/log.h"
#include "pico/time.h"
#include "pico/unique_id.h"

namespace {

std::uint64_t seed_from_board_id() noexcept
{
    pico_unique_board_id_t board_id{};
    pico_get_unique_board_id(&board_id);

    std::uint64_t seed = 0;
    for (const auto byte : board_id.id)
        seed = (seed << 8U) | byte;

    // SplitMix64 finalizer: spread nearby board IDs across unrelated PRNG seeds.
    seed ^= seed >> 30U;
    seed *= UINT64_C(0xbf58476d1ce4e5b9);
    seed ^= seed >> 27U;
    seed *= UINT64_C(0x94d049bb133111eb);
    seed ^= seed >> 31U;
    return seed;
}

} // namespace

namespace leos::cyphal {

static_assert(LEOS_CYPHAL_MCP_FILTER_COUNT == MCP251XFD_FILTER_COUNT,
              "MCP filter count must match the vendor driver");

const canard_vtable_t Node::vtable_ = {
    &Node::now,
    &Node::transmit,
    &Node::configure_filters,
};
const canard_mem_vtable_t Node::memory_vtable_ = { &Node::free, &Node::allocate };
Node* Node::irq_node_ = nullptr;

Node::Node(const leos_mcp251xfd_hw_t& hardware) noexcept {
    mcp_.hw = hardware;
}

eERRORRESULT Node::init(const leos_mcp251xfd_config_t& config, std::uint8_t node_id) noexcept {
    if (initialized_ || ((node_id != CANARD_NODE_ID_ANONYMOUS) &&
                         (node_id > CANARD_NODE_ID_MAX)))
        return ERR__PARAMETER_ERROR;

    heap_ = o1heapInit(heap_arena_, sizeof(heap_arena_));
    if (heap_ == nullptr)
        return ERR__OUT_OF_MEMORY;

    const canard_mem_t resource = { &memory_vtable_, heap_ };
    const canard_mem_set_t memory = { resource, resource, resource, resource, resource };
    if (!canard_new(&canard_, &vtable_, memory, 1U, tx_queue_capacity_,
                    seed_from_board_id(), LEOS_CYPHAL_MCP_FILTER_COUNT)) {
        return ERR__CONFIGURATION;
    }
    canard_.user_context = this;
    if ((node_id != CANARD_NODE_ID_ANONYMOUS) && !canard_set_node_id(&canard_, node_id)) {
        canard_destroy(&canard_);
        return ERR__PARAMETER_ERROR;
    }

    if (!external_irq_) {
        irq_node_ = this;
        gpio_set_irq_callback(&Node::picoIrqCallback);
    }

    const eERRORRESULT mcp_result = leos_mcp251xfd_init(&mcp_, &config);
    if (mcp_result != ERR_OK) {
        if (!external_irq_) {
            gpio_set_irq_callback(nullptr);
            irq_node_ = nullptr;
        }
        canard_destroy(&canard_);
        return mcp_result;
    }

    initialized_ = true;
    next_heartbeat_us_ = now(&canard_); // Publish on the first poll.
    LOG_DEBUG("Cyphal node initialized (ID=%u)", (unsigned)canard_.node_id);
    return ERR_OK;
}

bool Node::useExternalIrq() noexcept
{
    if (initialized_)
        return false;
    external_irq_ = true;
    return true;
}

void Node::picoIrqCallback(uint gpio, std::uint32_t event_mask) noexcept
{
    if (irq_node_ != nullptr)
        irq_node_->dispatchIrq(gpio, event_mask);
}

/* Memory Allocation Functions (from O1Heap) */

void Node::free(canard_mem_t memory, std::size_t, void* ptr) noexcept
{
    o1heapFree(static_cast<O1HeapInstance*>(memory.context), ptr);
}

void* Node::allocate(canard_mem_t memory, std::size_t size) noexcept
{
    return o1heapAllocate(static_cast<O1HeapInstance*>(memory.context), size);
}

bool Node::dispatchIrq(uint gpio, std::uint32_t event_mask) noexcept
{
    return leos_mcp251xfd_handle_gpio_irq(&mcp_, gpio, event_mask);
}

void Node::latchCanIrq() noexcept
{
    leos_mcp251xfd_irq_handler(&mcp_);
}

bool Node::setHealth(Health value) noexcept
{
    if (value > Health::Warning)
        return false;
    if (health_ != value) {
        health_ = value;
        LOG_DEBUG("Cyphal heartbeat health=%u", (unsigned)value);
    }
    return true;
}

bool Node::setMode(Mode value) noexcept
{
    if (value > Mode::SoftwareUpdate)
        return false;
    if (mode_ != value) {
        mode_ = value;
        LOG_DEBUG("Cyphal heartbeat mode=%u", (unsigned)value);
    }
    return true;
}

void Node::setVendorStatusCode(std::uint8_t value) noexcept
{
    if (vendor_status_code_ != value) {
        vendor_status_code_ = value;
        LOG_DEBUG("Cyphal heartbeat vendor status=%u", (unsigned)value);
    }
}

std::uint32_t Node::uptimeSeconds() const noexcept
{
    const std::uint64_t seconds = now(&canard_) / heartbeat_period_us_;
    return static_cast<std::uint32_t>(seconds > UINT32_MAX ? UINT32_MAX : seconds);
}

bool Node::publishHeartbeat(canard_us_t timestamp) noexcept
{
    const std::uint32_t uptime = uptimeSeconds();

    // --- SERIALIZE HEARTBEAT.1.0 ---
    // Nested Health and Mode are byte-aligned in DSDL; no generated headers
    // are needed for this fixed seven-byte payload.
    const std::uint8_t payload[7] = {
        static_cast<std::uint8_t>(uptime),
        static_cast<std::uint8_t>(uptime >> 8U),
        static_cast<std::uint8_t>(uptime >> 16U),
        static_cast<std::uint8_t>(uptime >> 24U),
        static_cast<std::uint8_t>(health_),
        static_cast<std::uint8_t>(mode_),
        vendor_status_code_,
    };
    const canard_bytes_chain_t bytes{{sizeof(payload), payload}, nullptr};
    if (!canard_publish_13b(&canard_, timestamp + heartbeat_period_us_, 1U,
                            canard_prio_nominal, heartbeat_subject_id_,
                            heartbeat_tid_, bytes, nullptr)) {
        if (!heartbeat_enqueue_failed_) {
            LOG_WARNING("Cyphal heartbeat enqueue failed; retrying");
            heartbeat_enqueue_failed_ = true;
        }
        return false;
    }

    if (heartbeat_enqueue_failed_)
        LOG_DEBUG("Cyphal heartbeat enqueue recovered");
    heartbeat_enqueue_failed_ = false;
    LOG_TRACE("Cyphal heartbeat queued uptime=%lu health=%u mode=%u status=%u tid=%u",
              (unsigned long)uptime, (unsigned)health_, (unsigned)mode_,
              (unsigned)vendor_status_code_, (unsigned)heartbeat_tid_);
    heartbeat_tid_ = static_cast<std::uint8_t>((heartbeat_tid_ + 1U) & CANARD_TRANSFER_ID_MAX);
    next_heartbeat_us_ = timestamp + heartbeat_period_us_;
    return true;
}

void Node::poll() noexcept
{
    if (!initialized_)
        return;

    // --- SERVICE MCP INTERRUPTS ---
    // This performs SPI work deferred by the GPIO callback. FIFO1 remains
    // owned by the receive loop below.
    leos_mcp251xfd_task(&mcp_);

    // --- RECEIVE CAN FRAMES ---
    // Keep the batch bounded so a busy bus cannot starve TX or other tasks.
    std::size_t received = 0;
    for (; received < max_rx_frames_per_poll_; received++) {
        std::uint8_t payload[64];
        std::uint32_t can_id = 0;
        std::size_t size = 0;
        const eERRORRESULT result = leos_mcp251xfd_receive(
            &mcp_, &can_id, payload, sizeof(payload), &size, nullptr);
        if (result == ERR__NO_DATA_AVAILABLE)
            break;
        if (result != ERR_OK) {
            LOG_ERROR("Cyphal RX stopped; MCP error: %d", result);
            break;
        }

        // Use Pico's 64-bit clock; the MCP's 32-bit timestamp can wrap.
        const canard_bytes_t frame{size, payload};
        if (!canard_ingest_frame(&canard_, now(&canard_), 0U, can_id, frame)) {
            LOG_ERROR("Cyphal rejected CAN frame 0x%08lx", (unsigned long)can_id);
            break;
        }
    }
    if (received != 0U)
        LOG_TRACE("Cyphal ingested %u CAN frames", (unsigned)received);

    // FIFO1 may have held the IRQ low while other MCP events were pending.
    leos_mcp251xfd_task(&mcp_);

    // --- SERVICE LIBCANARD ---
    // Heartbeat is the only publication owned by the node itself.
    const canard_us_t timestamp = now(&canard_);
    if (timestamp >= next_heartbeat_us_)
        publishHeartbeat(timestamp);

    // The TX callback returns false when TXQ is full; the next poll retries.
    // Polling also applies acceptance filters and expires old TX/RX state.
    canard_poll(&canard_, 1U);
}

/* Canard Driver Functions */

canard_us_t Node::now(const canard_t*) noexcept
{
    return static_cast<canard_us_t>(to_us_since_boot(get_absolute_time()));
}

bool Node::transmit(canard_t* self, void*, canard_us_t,
                    uint_least8_t iface_index, bool fd, std::uint32_t can_id,
                    canard_bytes_t data) noexcept
{
    if ((self == nullptr) || (iface_index != 0U) || !fd)
        return false; // TODO: support Classic CAN if the node needs it.
    auto* node = static_cast<Node*>(self->user_context);
    return (node != nullptr) &&
           (leos_mcp251xfd_send(&node->mcp_, can_id,
                                static_cast<const std::uint8_t*>(data.data), data.size) == ERR_OK);
}

bool Node::configure_filters(canard_t* self, std::size_t filter_count,
                             const canard_filter_t* filters) noexcept
{
    if ((self == nullptr) || (filter_count > LEOS_CYPHAL_MCP_FILTER_COUNT) ||
        ((filter_count != 0U) && (filters == nullptr)))
        return false;
    auto* node = static_cast<Node*>(self->user_context);
    if (node == nullptr)
        return false;

    // Replace the prior configuration, including when libcanard requests zero filters.
    for (std::size_t i = 0; i < LEOS_CYPHAL_MCP_FILTER_COUNT; i++) {
        if (MCP251XFD_DisableFilter(&node->mcp_.native,
                                   static_cast<eMCP251XFD_Filter>(i)) != ERR_OK)
            return false;
    }

    for (std::size_t i = 0; i < filter_count; ++i) {
        MCP251XFD_Filter filter{};
        filter.Filter = static_cast<eMCP251XFD_Filter>(i);
        filter.EnableFilter = true;
        filter.Match = MCP251XFD_MATCH_ONLY_EID;
        filter.PointTo = MCP251XFD_FIFO1;
        filter.AcceptanceID = filters[i].extended_can_id & filters[i].extended_mask;
        filter.AcceptanceMask = filters[i].extended_mask;
        filter.ExtendedID = true;
        if (MCP251XFD_ConfigureFilter(&node->mcp_.native, &filter) != ERR_OK)
            return false;
    }
    return true;
}

} // namespace leos::cyphal
