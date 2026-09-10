/**
 * @file thermal_governor.h
 * @brief DVFS-based thermal management policy.
 *
 * Five temperature zones with progressively aggressive throttling:
 *   NORMAL (<35°C) → WARM (35-38°C) → HOT (38-41°C) → CRITICAL (41-43°C) → EMERGENCY (>43°C)
 */
#pragma once

#include "core/config.h"
#include <string>
#include <chrono>
#include <atomic>

namespace orbit::core {

enum class ThermalZone : uint8_t {
    NORMAL = 0,   // All features, full clock
    WARM,         // Reduce SLM batch frequency
    HOT,          // SLM deferred to post-call, CPU downclocked
    CRITICAL,     // SLM disabled, ASR-only, aggressive downclock
    EMERGENCY,    // Graceful shutdown
};

const char* thermal_zone_to_string(ThermalZone zone);

class ThermalGovernor {
public:
    explicit ThermalGovernor(const Config& config);
    ~ThermalGovernor() = default;

    /**
     * @brief Poll thermal sensors and update system state.
     *        Adjusts CPU/NPU frequencies via sysfs DVFS interface.
     *        Should be called every ~1 second.
     */
    void poll();

    /**
     * @brief Reload thermal thresholds from config (hot-reload support).
     */
    void reload(const Config& config);

    /**
     * @brief Get the current thermal zone.
     */
    ThermalZone current_zone() const;

    /**
     * @brief Get the current temperature in degrees Celsius.
     */
    float current_temp_c() const;

    /**
     * @brief Get the SLM inference batch interval in seconds.
     *        Returns 30 in NORMAL, 45 in WARM, INT_MAX in HOT/CRITICAL.
     */
    int get_slm_interval_s() const;

    /**
     * @brief Check if the SLM should be completely disabled.
     */
    bool is_slm_disabled() const;

    /**
     * @brief Check if we're in CRITICAL or EMERGENCY zone.
     */
    bool is_critical() const;

    /**
     * @brief Check if we've hit EMERGENCY and must shut down.
     */
    bool is_emergency() const;

    /**
     * @brief Get peak temperature recorded since boot.
     */
    float peak_temp_c() const;

private:
    /**
     * @brief Read temperature from sysfs thermal zone.
     */
    float read_temperature() const;

    /**
     * @brief Set CPU frequency via sysfs cpufreq.
     */
    void set_cpu_frequency(uint32_t freq_khz);

    /**
     * @brief Set NPU frequency via sysfs devfreq.
     */
    void set_npu_frequency(uint32_t freq_khz);

    /**
     * @brief Determine thermal zone from temperature.
     */
    ThermalZone classify_temperature(float temp_c) const;

    // Configuration
    std::string thermal_zone_path_;
    std::string cpu_freq_path_;
    std::string npu_freq_path_;

    // Thresholds (°C)
    float threshold_warm_ = 35.0f;
    float threshold_hot_ = 38.0f;
    float threshold_critical_ = 41.0f;
    float threshold_emergency_ = 43.0f;

    // CPU frequencies (kHz)
    uint32_t freq_full_ = 2200000;       // 2.2 GHz
    uint32_t freq_hot_ = 1800000;        // 1.8 GHz
    uint32_t freq_critical_ = 1400000;   // 1.4 GHz

    // NPU frequencies (kHz)
    uint32_t npu_freq_full_ = 900000;    // 900 MHz
    uint32_t npu_freq_idle_ = 300000;    // 300 MHz

    // Runtime state
    std::atomic<ThermalZone> current_zone_{ThermalZone::NORMAL};
    std::atomic<float> current_temp_{0.0f};
    float peak_temp_ = 0.0f;

    // Hysteresis: prevent rapid zone oscillation
    float hysteresis_margin_ = 2.0f;  // Must drop 2°C below threshold to exit zone
    ThermalZone last_applied_zone_ = ThermalZone::NORMAL;
};

}  // namespace orbit::core
