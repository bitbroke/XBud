/**
 * @file thermal_governor.cpp
 * @brief DVFS-based thermal management implementation.
 */

#include "core/thermal_governor.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <climits>

namespace orbit::core {

const char* thermal_zone_to_string(ThermalZone zone) {
    switch (zone) {
        case ThermalZone::NORMAL:    return "NORMAL";
        case ThermalZone::WARM:      return "WARM";
        case ThermalZone::HOT:       return "HOT";
        case ThermalZone::CRITICAL:  return "CRITICAL";
        case ThermalZone::EMERGENCY: return "EMERGENCY";
        default:                     return "UNKNOWN";
    }
}

ThermalGovernor::ThermalGovernor(const Config& config) {
    reload(config);
}

void ThermalGovernor::reload(const Config& config) {
    thermal_zone_path_ = config.get_string(
        "thermal.sensor_path", "/sys/class/thermal/thermal_zone0/temp");
    cpu_freq_path_ = config.get_string(
        "thermal.cpu_freq_path",
        "/sys/devices/system/cpu/cpufreq/policy0/scaling_max_freq");
    npu_freq_path_ = config.get_string(
        "thermal.npu_freq_path",
        "/sys/class/devfreq/fdab0000.npu/max_freq");

    threshold_warm_ = config.get_float("thermal.threshold_warm", 35.0f);
    threshold_hot_ = config.get_float("thermal.threshold_hot", 38.0f);
    threshold_critical_ = config.get_float("thermal.threshold_critical", 41.0f);
    threshold_emergency_ = config.get_float("thermal.threshold_emergency", 43.0f);

    freq_full_ = static_cast<uint32_t>(config.get_int("thermal.freq_full_khz", 2200000));
    freq_hot_ = static_cast<uint32_t>(config.get_int("thermal.freq_hot_khz", 1800000));
    freq_critical_ = static_cast<uint32_t>(config.get_int("thermal.freq_critical_khz", 1400000));

    hysteresis_margin_ = config.get_float("thermal.hysteresis_margin", 2.0f);
}

void ThermalGovernor::poll() {
    float temp = read_temperature();
    current_temp_.store(temp, std::memory_order_release);
    peak_temp_ = std::max(peak_temp_, temp);

    ThermalZone new_zone = classify_temperature(temp);

    // Apply hysteresis: only transition DOWN if temperature is below
    // threshold minus hysteresis margin
    if (new_zone < last_applied_zone_) {
        float required_temp = 0.0f;
        switch (last_applied_zone_) {
            case ThermalZone::WARM:
                required_temp = threshold_warm_ - hysteresis_margin_;
                break;
            case ThermalZone::HOT:
                required_temp = threshold_hot_ - hysteresis_margin_;
                break;
            case ThermalZone::CRITICAL:
                required_temp = threshold_critical_ - hysteresis_margin_;
                break;
            default:
                break;
        }
        if (temp > required_temp) {
            return;  // Stay in current zone (hysteresis)
        }
    }

    if (new_zone != last_applied_zone_) {
        std::cout << "[thermal] Zone change: "
                  << thermal_zone_to_string(last_applied_zone_) << " → "
                  << thermal_zone_to_string(new_zone)
                  << " (temp=" << temp << "°C)" << std::endl;

        // Apply DVFS changes
        switch (new_zone) {
            case ThermalZone::NORMAL:
                set_cpu_frequency(freq_full_);
                set_npu_frequency(npu_freq_full_);
                break;
            case ThermalZone::WARM:
                // No frequency change, just reduce SLM interval
                break;
            case ThermalZone::HOT:
                set_cpu_frequency(freq_hot_);
                break;
            case ThermalZone::CRITICAL:
                set_cpu_frequency(freq_critical_);
                set_npu_frequency(npu_freq_idle_);
                break;
            case ThermalZone::EMERGENCY:
                set_cpu_frequency(freq_critical_);
                set_npu_frequency(npu_freq_idle_);
                std::cerr << "[thermal] EMERGENCY: Initiating shutdown at "
                          << temp << "°C" << std::endl;
                break;
        }

        last_applied_zone_ = new_zone;
        current_zone_.store(new_zone, std::memory_order_release);
    }
}

ThermalZone ThermalGovernor::current_zone() const {
    return current_zone_.load(std::memory_order_acquire);
}

float ThermalGovernor::current_temp_c() const {
    return current_temp_.load(std::memory_order_acquire);
}

int ThermalGovernor::get_slm_interval_s() const {
    switch (current_zone()) {
        case ThermalZone::NORMAL:    return 30;
        case ThermalZone::WARM:      return 45;
        case ThermalZone::HOT:       return INT_MAX;  // Post-call only
        case ThermalZone::CRITICAL:  return INT_MAX;  // Disabled
        case ThermalZone::EMERGENCY: return INT_MAX;  // Disabled
        default:                     return 30;
    }
}

bool ThermalGovernor::is_slm_disabled() const {
    auto zone = current_zone();
    return zone == ThermalZone::CRITICAL || zone == ThermalZone::EMERGENCY;
}

bool ThermalGovernor::is_critical() const {
    auto zone = current_zone();
    return zone == ThermalZone::CRITICAL || zone == ThermalZone::EMERGENCY;
}

bool ThermalGovernor::is_emergency() const {
    return current_zone() == ThermalZone::EMERGENCY;
}

float ThermalGovernor::peak_temp_c() const {
    return peak_temp_;
}

float ThermalGovernor::read_temperature() const {
    std::ifstream file(thermal_zone_path_);
    if (!file.is_open()) {
        std::cerr << "[thermal] Cannot read: " << thermal_zone_path_ << std::endl;
        return 0.0f;
    }

    int temp_mc = 0;
    file >> temp_mc;
    return static_cast<float>(temp_mc) / 1000.0f;
}

void ThermalGovernor::set_cpu_frequency(uint32_t freq_khz) {
    std::ofstream file(cpu_freq_path_);
    if (file.is_open()) {
        file << freq_khz;
        std::cout << "[thermal] CPU max freq → " << freq_khz / 1000 << " MHz" << std::endl;
    } else {
        std::cerr << "[thermal] Cannot write CPU freq: " << cpu_freq_path_ << std::endl;
    }
}

void ThermalGovernor::set_npu_frequency(uint32_t freq_khz) {
    std::ofstream file(npu_freq_path_);
    if (file.is_open()) {
        file << freq_khz;
        std::cout << "[thermal] NPU max freq → " << freq_khz / 1000 << " MHz" << std::endl;
    } else {
        std::cerr << "[thermal] Cannot write NPU freq: " << npu_freq_path_ << std::endl;
    }
}

ThermalZone ThermalGovernor::classify_temperature(float temp_c) const {
    if (temp_c >= threshold_emergency_) return ThermalZone::EMERGENCY;
    if (temp_c >= threshold_critical_)  return ThermalZone::CRITICAL;
    if (temp_c >= threshold_hot_)       return ThermalZone::HOT;
    if (temp_c >= threshold_warm_)      return ThermalZone::WARM;
    return ThermalZone::NORMAL;
}

}  // namespace orbit::core
