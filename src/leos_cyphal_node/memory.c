/**
 * @file cyphal_memory.c
 * @brief Deterministic memory allocator implementation using o1heap for libcanard.
 */

#include "leos/cyphal/memory.h"
#include "leos/log.h"
#include "o1heap.h"
#include "pico/stdlib.h"

#define O1HEAP_ARENA_SIZE (16 * 1024)
static uint8_t o1heap_arena[O1HEAP_ARENA_SIZE] __attribute__((aligned(O1HEAP_ALIGNMENT)));
static O1HeapInstance* heap = NULL;

bool canard_memory_init()
{
    heap = o1heapInit(o1heap_arena, sizeof(o1heap_arena));
    if (heap == NULL) {
        // Initialization failed
        LOG_ERROR("O1Heap init failed");
        return false;
    }
    LOG_INFO("O1Heap initialized successfully");
    return true;
}

static void* heap_allocate(void* const user_reference, const size_t amount)
{
    (void)user_reference;
    return o1heapAllocate(heap, amount);
}

static void heap_deallocate(void* const user_reference, const size_t amount, void* const pointer)
{
    (void)user_reference;
    (void)amount;

    o1heapFree(heap, pointer);
}

struct CanardMemoryResource canard_memory_resource()
{
    struct CanardMemoryResource mem = {
        .user_reference = NULL,
        .allocate = heap_allocate,
        .deallocate = heap_deallocate
    };
    return mem;
}
