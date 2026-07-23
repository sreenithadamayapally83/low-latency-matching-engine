#ifndef FIXED_ARENA_HPP
#define FIXED_ARENA_HPP

#include "Types.hpp"
#include <vector>
#include <stdexcept>
#include <cstddef>

template <size_t Capacity>
class FixedArena {
public:
    FixedArena() {
        // Pre-allocating continuous memory upfront during startup
        pool_.resize(Capacity);
        free_list_.reserve(Capacity);
        
        // Populating free list with pointers to pre-allocated elements
        for (size_t i = 0; i < Capacity; ++i) {
            free_list_.push_back(&pool_[i]);
        }
    }

    // Non-copyable to prevent accidental expensive memory duplicates
    FixedArena(const FixedArena&) = delete;
    FixedArena& operator=(const FixedArena&) = delete;

    // O(1) allocation - Zero malloc/new overhead
    [[nodiscard]] Order* allocate() noexcept {
        if (free_list_.empty()) [[unlikely]] {
            return nullptr; // Arena exhausted
        }
        Order* ptr = free_list_.back();
        free_list_.pop_back();
        return ptr;
    }

    // O(1) deallocation - Return memory pointer back to free list
    void deallocate(Order* ptr) noexcept {
        free_list_.push_back(ptr);
    }

    [[nodiscard]] size_t free_capacity() const noexcept { 
        return free_list_.size(); 
    }

    [[nodiscard]] constexpr size_t total_capacity() const noexcept { 
        return Capacity; 
    }

private:
    std::vector<Order> pool_;
    std::vector<Order*> free_list_;
};

#endif