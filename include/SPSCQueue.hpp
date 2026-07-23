#ifndef SPSC_QUEUE_HPP
#define SPSC_QUEUE_HPP

#include <atomic>
#include <vector>
#include <cstddef>
#include <cassert>

template <typename T, size_t Capacity>
class SPSCQueue {
    // Capacity must be a power of 2 for fast bitwise modulo: index = sequence & (Capacity - 1)
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2!");

public:
    SPSCQueue() : buffer_(Capacity) {}

    // Non-copyable
    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;

    // Producer thread calls this
    template <typename... Args>
    bool emplace(Args&&... args) noexcept {
        const uint64_t current_tail = tail_.load(std::memory_order_relaxed);
        
        // Check if queue is full
        if (current_tail - head_cache_ >= Capacity) {
            head_cache_ = head_.load(std::memory_order_acquire);
            if (current_tail - head_cache_ >= Capacity) {
                return false; // Queue is full
            }
        }

        // Construct object directly in-place inside buffer slot
        buffer_[current_tail & kMask] = T{std::forward<Args>(args)...};
        
        // Publish write to consumer thread
        tail_.store(current_tail + 1, std::memory_order_release);
        return true;
    }

    // Consumer (Engine) thread calls this
    bool pop(T& item) noexcept {
        const uint64_t current_head = head_.load(std::memory_order_relaxed);

        // Checking if queue is empty
        if (current_head == tail_cache_) {
            tail_cache_ = tail_.load(std::memory_order_acquire);
            if (current_head == tail_cache_) {
                return false; // Queue is empty
            }
        }

        item = buffer_[current_head & kMask];
        
        // Updating head cursor
        head_.store(current_head + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed);
    }

private:
    static constexpr size_t kMask = Capacity - 1;

    // Ring buffer storage
    std::vector<T> buffer_;

    // Prevent False Sharing: Put head and tail on SEPARATE 64-byte cache lines!
    alignas(64) std::atomic<uint64_t> head_{0};
    uint64_t tail_cache_{0}; // Consumer local cache of tail cursor

    alignas(64) std::atomic<uint64_t> tail_{0};
    uint64_t head_cache_{0}; // Producer local cache of head cursor
};

#endif