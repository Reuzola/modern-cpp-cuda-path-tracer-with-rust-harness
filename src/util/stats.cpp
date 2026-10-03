#include "pt/util/stats.hpp"
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace pt {

namespace {

// Owns every slot for the life of the process. unique_ptr, not the slots by
// value: a growing vector relocates its elements, and each thread holds a raw
// pointer to its own slot.
struct SlotRegistry {
    std::mutex mutex;
    std::vector<std::unique_ptr<detail::TraversalSlot>> slots; // Guarded by mutex
};

// constinit: constant-initialized before any code runs, so no thread can count
// into it before it exists. Destroyed after main returns, by which point every
// pool has joined its workers.
constinit SlotRegistry slot_registry;

} // namespace

TraversalStats& detail::register_traversal_slot() noexcept {
    std::unique_ptr<TraversalSlot> slot = std::make_unique<TraversalSlot>();
    TraversalSlot* raw = slot.get();
    {
        const std::scoped_lock lock(slot_registry.mutex);
        slot_registry.slots.push_back(std::move(slot));
    }
    traversal_slot = raw;
    return raw->stats;
}

TraversalStats traversal_snapshot() noexcept {
    const std::scoped_lock lock(slot_registry.mutex);
    TraversalStats total{};

    for (const auto& slot : slot_registry.slots) {
        total += slot->stats;
    }
    return total;
}

void reset_traversal_stats() noexcept {
    const std::scoped_lock lock(slot_registry.mutex);

    for (const auto& slot : slot_registry.slots) {
        slot->stats = TraversalStats{};
    }
}

} // namespace pt
