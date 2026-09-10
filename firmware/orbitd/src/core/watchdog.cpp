/**
 * @file watchdog.cpp
 * @brief Hardware and software watchdog implementation.
 */

#include "core/watchdog.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/watchdog.h>
#include <iostream>
#include <cstring>

namespace orbit::core {

// Static thread health array
std::array<Watchdog::ThreadHealth, Watchdog::NUM_THREADS> Watchdog::thread_health_;

Watchdog::Watchdog(const Config& config)
    : config_(config)
{
    timeout_seconds_ = config.get_int("watchdog.timeout_seconds", 30);
    max_stale_duration_ = std::chrono::seconds(
        config.get_int("watchdog.max_stale_seconds", 10)
    );
}

Watchdog::~Watchdog() {
    disable();
}

bool Watchdog::initialize() {
    if (!config_.get_bool("watchdog.hardware_enabled", true)) {
        std::cout << "[watchdog] Hardware watchdog disabled by config" << std::endl;
        return false;
    }

    watchdog_fd_ = open("/dev/watchdog", O_WRONLY);
    if (watchdog_fd_ < 0) {
        std::cerr << "[watchdog] Failed to open /dev/watchdog: "
                  << strerror(errno) << std::endl;
        return false;
    }

    // Set timeout
    if (ioctl(watchdog_fd_, WDIOC_SETTIMEOUT, &timeout_seconds_) < 0) {
        std::cerr << "[watchdog] Failed to set timeout: "
                  << strerror(errno) << std::endl;
        close(watchdog_fd_);
        watchdog_fd_ = -1;
        return false;
    }

    std::cout << "[watchdog] Hardware watchdog active, timeout="
              << timeout_seconds_ << "s" << std::endl;
    return true;
}

void Watchdog::feed() {
    if (watchdog_fd_ >= 0) {
        // Write any byte to pet the watchdog
        int dummy = 0;
        if (ioctl(watchdog_fd_, WDIOC_KEEPALIVE, &dummy) < 0) {
            std::cerr << "[watchdog] Failed to feed watchdog: "
                      << strerror(errno) << std::endl;
        }
    }
}

void Watchdog::heartbeat(ThreadID thread) {
    auto idx = static_cast<size_t>(thread);
    if (idx < NUM_THREADS) {
        thread_health_[idx].counter.fetch_add(1, std::memory_order_relaxed);
    }
}

void Watchdog::check_thread_health() {
    static const char* thread_names[] = {
        "AUDIO", "VAD_ASR", "SLM", "THERMAL", "BLE"
    };

    for (size_t i = 0; i < NUM_THREADS; ++i) {
        uint64_t current = thread_health_[i].counter.load(std::memory_order_relaxed);

        if (current == thread_health_[i].last_seen) {
            // Thread has not progressed since last check
            std::cerr << "[watchdog] WARNING: Thread " << thread_names[i]
                      << " appears stalled (counter=" << current << ")"
                      << std::endl;

            // Critical threads trigger more aggressive response
            if (i == static_cast<size_t>(ThreadID::AUDIO) ||
                i == static_cast<size_t>(ThreadID::VAD_ASR)) {
                std::cerr << "[watchdog] CRITICAL: Core thread stalled, "
                          << "system may need restart" << std::endl;
                // In production, this would trigger a controlled restart
            }
        }

        thread_health_[i].last_seen = current;
    }
}

void Watchdog::disable() {
    if (watchdog_fd_ >= 0) {
        // Write 'V' (magic close character) to gracefully disable watchdog
        const char magic = 'V';
        write(watchdog_fd_, &magic, 1);
        close(watchdog_fd_);
        watchdog_fd_ = -1;
        std::cout << "[watchdog] Hardware watchdog disabled" << std::endl;
    }
}

}  // namespace orbit::core
