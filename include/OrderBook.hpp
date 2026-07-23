#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include "Types.hpp"
#include "FixedArena.hpp"
#include <array>
#include <iostream>

// Structures for Price-Time Priority FIFO queue at a single price point
struct PriceLevel {
    uint32_t price{0};
    uint32_t total_volume{0};
    Order* head{nullptr};
    Order* tail{nullptr};

    void append(Order* order) noexcept {
        order->next = nullptr;
        order->prev = tail;
        if (tail) {
            tail->next = order;
        } else {
            head = order;
        }
        tail = order;
        total_volume += order->qty;
    }

    void remove(Order* order) noexcept {
        if (order->prev) {
            order->prev->next = order->next;
        } else {
            head = order->next;
        }

        if (order->next) {
            order->next->prev = order->prev;
        } else {
            tail = order->prev;
        }

        total_volume -= order->qty;
        order->prev = nullptr;
        order->next = nullptr;
    }
};

template <size_t MaxOrders = 1000000, uint32_t MaxPriceLevels = 20000>
class OrderBook {
public:
    OrderBook() = default;

    // Zero-allocation order creation via internal arena
    Order* create_order(uint64_t id, uint32_t price, uint32_t qty, Side side, OrderType type) noexcept {
        Order* order = arena_.allocate();
        if (!order) [[unlikely]] return nullptr;

        order->order_id = id;
        order->price = price;
        order->qty = qty;
        order->side = side;
        order->type = type;
        order->prev = nullptr;
        order->next = nullptr;
        return order;
    }

    void free_order(Order* order) noexcept {
        arena_.deallocate(order);
    }

    // Process incoming order against opposite side of the book
    void add_order(uint64_t id, uint32_t price, uint32_t qty, Side side, OrderType type) noexcept {
        Order* order = create_order(id, price, qty, side, type);
        if (!order) return;

        if (side == Side::BUY) {
            match_buy(order);
        } else {
            match_sell(order);
        }
    }

private:
    void match_buy(Order* order) noexcept {
        // Match against lowest active SELL prices (Asks)
        while (order->qty > 0 && best_ask_price_ <= order->price && best_ask_price_ > 0) {
            PriceLevel& level = asks_[best_ask_price_];
            Order* current = level.head;

            while (current && order->qty > 0) {
                Order* next_order = current->next;
                uint32_t match_qty = std::min(order->qty, current->qty);

                // Execute trade at current price
                order->qty -= match_qty;
                current->qty -= match_qty;
                level.total_volume -= match_qty;

                // Fully matched order removed in O(1)
                if (current->qty == 0) {
                    level.remove(current);
                    free_order(current);
                }

                current = next_order;
            }

            // Move best_ask_price_ pointer if level is cleared
            if (level.head == nullptr) {
                update_best_ask();
            }
        }

        // If resting volume remains, place on BUY book (Bids)
        if (order->qty > 0 && order->type == OrderType::LIMIT) {
            bids_[order->price].price = order->price;
            bids_[order->price].append(order);
            if (order->price > best_bid_price_) {
                best_bid_price_ = order->price;
            }
        } else if (order->qty == 0) {
            free_order(order);
        }
    }

    void match_sell(Order* order) noexcept {
        // Match against highest active BUY prices (Bids)
        while (order->qty > 0 && best_bid_price_ >= order->price && best_bid_price_ > 0) {
            PriceLevel& level = bids_[best_bid_price_];
            Order* current = level.head;

            while (current && order->qty > 0) {
                Order* next_order = current->next;
                uint32_t match_qty = std::min(order->qty, current->qty);

                // Execute trade
                order->qty -= match_qty;
                current->qty -= match_qty;
                level.total_volume -= match_qty;

                if (current->qty == 0) {
                    level.remove(current);
                    free_order(current);
                }

                current = next_order;
            }

            if (level.head == nullptr) {
                update_best_bid();
            }
        }

        // If resting volume remains, place on SELL book (Asks)
        if (order->qty > 0 && order->type == OrderType::LIMIT) {
            asks_[order->price].price = order->price;
            asks_[order->price].append(order);
            if (best_ask_price_ == 0 || order->price < best_ask_price_) {
                best_ask_price_ = order->price;
            }
        } else if (order->qty == 0) {
            free_order(order);
        }
    }

    void update_best_ask() noexcept {
        for (uint32_t p = best_ask_price_ + 1; p < MaxPriceLevels; ++p) {
            if (asks_[p].head != nullptr) {
                best_ask_price_ = p;
                return;
            }
        }
        best_ask_price_ = 0;
    }

    void update_best_bid() noexcept {
        if (best_bid_price_ == 0) return;
        for (uint32_t p = best_bid_price_ - 1; p > 0; --p) {
            if (bids_[p].head != nullptr) {
                best_bid_price_ = p;
                return;
            }
        }
        best_bid_price_ = 0;
    }

public:
    [[nodiscard]] uint32_t best_bid() const noexcept { return best_bid_price_; }
    [[nodiscard]] uint32_t best_ask() const noexcept { return best_ask_price_; }

private:
    FixedArena<MaxOrders> arena_;
    std::array<PriceLevel, MaxPriceLevels> bids_{};
    std::array<PriceLevel, MaxPriceLevels> asks_{};
    uint32_t best_bid_price_{0};
    uint32_t best_ask_price_{0};
};

#endif