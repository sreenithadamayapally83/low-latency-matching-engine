#ifndef THREAD_UTILS_HPP
#define THREAD_UTILS_HPP

#include <pthread.h>
#include <sched.h>
#include <iostream>

namespace ThreadUtils {

    // Pins the calling thread strictly to a specific logical CPU core
    inline bool pin_thread_to_core(int core_id) noexcept {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core_id, &cpuset);

        pthread_t current_thread = pthread_self();
        int rc = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);

        if (rc != 0) {
            std::cerr << "Failed to pin thread to core " << core_id << ". Error code: " << rc << std::endl;
            return false;
        }

        return true;
    }

}

#endif