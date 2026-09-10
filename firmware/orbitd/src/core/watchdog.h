/**
 * @file watchdog.h
 * @brief Hardware and software watchdog management.
 *
 * Two-level watchdog architecture:
 *   1. Hardware watchdog (/dev/watchdog) — reboots system if daemon dies
 *   2. Software watchdog — monitors thread health via heartbeat counters
 */
#pragma once

#include "core/config.h"
#include <atomic>
#include <array>
#include <chrono>
#include <string>

namespace orbit::core {

/**
 * @brief Thread identifiers for health monitoring.
 */
enum class ThreadID : uint8_t {
    AUDIO = 0,
    VAD_ASR,
    SLM,
    THERMAL,
    BLE,
    COUNT  // Sentinel
};

class Watchdog {
public:
    explicit Watchdog(const Config& config);
    ~Watchdog();

    /**
     * @brief Open the hardware watchdog device and configure timeout.
     * @return true if hardware watchdog is available and configured.
     */
    bool initialize();

    /**
     * @brief Feed (pet) the hardware watchdog to prevent system reboot.
     *        Must be called at intervals less than the watchdog timeout.
     */
    void feed();

    /**
     * @brief Called by each monitored thread to report it is alive.
     */
    static void heartbeat(ThreadID thread);

    /**
     * @brief Check all thread heartbeats. Logs warnings for stale threads.
     *        If a critical thread (AUDIO, VAD_ASR) is stale, triggers recovery.
     */
    void check_thread_health();

    /**
     * @brief Disable the hardware watchdog (called during graceful shutdown).
     */
    void disable();

private:
    const Config& config_;
    int watchdog_fd_ = -1;
    int timeout_seconds_ = 30;

    struct ThreadHealth {
        std::atomic<uint64_t> counter{0};
        uint64_t last_seen{0};
    };

    static constexpr size_t NUM_THREADS = static_cast<size_t>(ThreadID::COUNT);
    static std::array<ThreadHealth, NUM_THREADS> thread_health_;

    std::chrono::seconds max_stale_duration_{10};
};

}  // namespace orbit::core
