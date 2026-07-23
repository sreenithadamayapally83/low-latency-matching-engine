#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <thread>
#include <numeric>
#include <cassert>

#include "Types.hpp"
#include "FixedArena.hpp"
#include "SPSCQueue.hpp"
#include "ThreadUtils.hpp"
#include "OrderBook.hpp"

int main() {
    std::cout << "  HFT LOW-LATENCY MATCHING ENGINE BENCHMARK (C++20)" << std::endl;

    constexpr size_t QUEUE_CAPACITY = 8192;
    constexpr size_t NUM_ORDERS = 1000000; // 1 Million Orders

    SPSCQueue<Order, QUEUE_CAPACITY> queue;
    std::vector<uint64_t> latencies_ns;
    latencies_ns.reserve(NUM_ORDERS);

    // Warm-up order book to pre-allocate caches
    OrderBook<NUM_ORDERS + 100, 20000> book;

    // Synchronized start signal
    std::atomic<bool> start_flag{false};

    // Consumer (Core Matching Engine Thread)
    std::thread engine_thread([&]() {
        // Pin engine to Core 1 (or 0)
        int core_id = (std::thread::hardware_concurrency() > 1) ? 1 : 0;
        ThreadUtils::pin_thread_to_core(core_id);

        while (!start_flag.load(std::memory_order_relaxed)) {
            #if defined(__x86_64__) || defined(_M_X64)
            __builtin_ia32_pause();
            #endif
        }

        Order order;
        size_t processed = 0;

        while (processed < NUM_ORDERS) {
            if (queue.pop(order)) {
                auto start_time = std::chrono::high_resolution_clock::now();

                // CORE MATCHING HOT PATH
                book.add_order(order.order_id, order.price, order.qty, order.side, order.type);

                auto end_time = std::chrono::high_resolution_clock::now();
                uint64_t latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
                latencies_ns.push_back(latency);

                ++processed;
            } else {
                #if defined(__x86_64__) || defined(_M_X64)
                __builtin_ia32_pause();
                #endif
            }
        }
    });

    // Producer (Gateway Ingress Thread)
    std::thread producer_thread([&]() {
        ThreadUtils::pin_thread_to_core(0);

        // Pre-generate order stream
        std::vector<Order> test_orders(NUM_ORDERS);
        for (size_t i = 0; i < NUM_ORDERS; ++i) {
            test_orders[i].order_id = i + 1;
            test_orders[i].price = 10000 + (i % 20); // Fluctuating prices
            test_orders[i].qty = 10;
            test_orders[i].side = (i % 2 == 0) ? Side::BUY : Side::SELL;
            test_orders[i].type = OrderType::LIMIT;
        }

        // Trigger start
        start_flag.store(true, std::memory_order_release);
        auto benchmark_start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < NUM_ORDERS; ++i) {
            while (!queue.emplace(test_orders[i])) {
                #if defined(__x86_64__) || defined(_M_X64)
                __builtin_ia32_pause();
                #endif
            }
        }

        engine_thread.join();
        auto benchmark_end = std::chrono::high_resolution_clock::now();

        double total_time_sec = std::chrono::duration<double>(benchmark_end - benchmark_start).count();
        double throughput = NUM_ORDERS / total_time_sec;

        // Sort latencies for percentile calculations
        std::sort(latencies_ns.begin(), latencies_ns.end());

        uint64_t p50  = latencies_ns[NUM_ORDERS * 0.50];
        uint64_t p90  = latencies_ns[NUM_ORDERS * 0.90];
        uint64_t p99  = latencies_ns[NUM_ORDERS * 0.99];
        uint64_t p999 = latencies_ns[NUM_ORDERS * 0.999];

        uint64_t sum = std::accumulate(latencies_ns.begin(), latencies_ns.end(), 0ULL);
        double avg_lat = static_cast<double>(sum) / NUM_ORDERS;

        std::cout << "\n--- BENCHMARK RESULTS (" << NUM_ORDERS << " Orders) ---" << std::endl;
        std::cout << "Total Execution Time: " << total_time_sec << " seconds" << std::endl;
        std::cout << "Engine Throughput   : " << static_cast<uint64_t>(throughput) << " orders/sec" << std::endl;
        std::cout << "----------------------------------------------------------" << std::endl;
        std::cout << "Mean Latency        : " << avg_lat << " ns" << std::endl;
        std::cout << "P50 Latency (Median): " << p50 << " ns" << std::endl;
        std::cout << "P90 Latency         : " << p90 << " ns" << std::endl;
        std::cout << "P99 Latency         : " << p99 << " ns" << std::endl;
        std::cout << "P99.9 Latency (Tail): " << p999 << " ns" << std::endl;
    });

    producer_thread.join();

    return 0;
}