#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstdint>
#include <cstddef>

// Enums fit in 1 byte to keep the struct compact
enum class Side : uint8_t { 
    BUY  = 0, 
    SELL = 1 
};

enum class OrderType : uint8_t { 
    LIMIT  = 0, 
    MARKET = 1 
};

// Aligning to 64 bytes (1 CPU cache line) to prevent line split penalty
struct alignas(64) Order {
    uint64_t order_id{0};
    uint32_t price{0};     // Fixed-point integer
    uint32_t qty{0};
    Side side{Side::BUY};
    OrderType type{OrderType::LIMIT};
    
    // Intrusive doubly-linked list pointers for O(1) order book manipulation later
    Order* prev{nullptr};
    Order* next{nullptr};
};

#endif