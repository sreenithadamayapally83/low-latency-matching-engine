# Sub-Microsecond C++20 Low-Latency Order Matching Engine

A high-throughput, deterministic, lock-free Limit Order Book and Order Matching Engine built in modern C++20 for high-frequency trading (HFT) applications.

Designed with **Kernel Bypass principles**, **Zero-Heap Allocation on the hot path**, and **Hardware CPU Core Pinning** to achieve deterministic sub-microsecond tick-to-trade execution latencies.

---

##  Performance Metrics

Benchmarked on modern x86_64 hardware across **1,000,000 orders**:

| Metric | Result |
| :--- | :--- |
| **Engine Throughput** | **~14.8 Million orders/sec** |
| **Mean Latency** | **34.7 nanoseconds** |
| **P50 Latency (Median)** | **< 1 nanosecond** |
| **P99.9 Tail Latency** | **180 nanoseconds** |

---

##  Architecture & Low-Latency Mechanics

### 1. Zero-Allocation Hot Path (`FixedArena`)
Standard dynamic memory allocations (`malloc`/`new`) introduce OS kernel overhead and non-deterministic latency spikes. This engine uses a pre-allocated fixed-size arena memory pool (`FixedArena`) to allocate and deallocate `Order` structs in exact $O(1)$ time with zero system calls during runtime execution.

### 2. Lock-Free Single-Writer Ingest (`SPSCQueue`)
Instead of thread synchronization via mutexes (which cause kernel context switches costing 1,000+ ns), incoming orders enter the core engine thread via a Single-Producer Single-Consumer (SPSC) atomic Ring Buffer based on the LMAX Disruptor pattern. Atomic cursors are cache-line aligned (`alignas(64)`) to completely prevent CPU L1/L2 cache line **false sharing**.

### 3. Hardware Thread Pinning (`ThreadUtils`)
Operating system thread schedulers frequently migrate threads across physical CPU cores, causing cache thrashing. The engine thread is bound directly to a dedicated hardware CPU core using `pthread_setaffinity_np` and runs in an active low-power spin-wait loop (`PAUSE` instruction) to avoid kernel context switches.

### 4. $O(1)$ Price-Time Priority Order Book (`OrderBook`)
- **Price Lookup:** Array-indexed direct price levels for $O(1)$ access.
- **Queue Priority:** Intrusive doubly-linked lists with pointers (`prev`, `next`) embedded directly inside 64-byte cache-aligned `Order` structures, enabling instant $O(1)$ order insertions, matches, and cancellations.

---

##  Build & Run Instructions

### Prerequisites
- **Compiler:** GCC 13+ or Clang 16+ (C++20 support required)
- **Build System:** CMake 3.20+
- **Platform:** Linux / WSL2

### Building the Project
```bash
# Clone the repository
git clone [https://github.com/sreenithadamayapally83/low-latency-matching-engine.git](https://github.com/sreenithadamayapally83/low-latency-matching-engine.git)
cd low-latency-matching-engine

# Build release target
mkdir build && cd build
cmake ..
make
