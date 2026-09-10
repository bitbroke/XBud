/**
 * @file energy_detector.cpp
 * @brief Dual-channel energy ratio detector implementation.
 */

#include "vad/energy_detector.h"
#include <cmath>

namespace orbit::vad {

void EnergyDetector::process(const std::vector<int16_t>& tx_samples,
                              const std::vector<int16_t>& rx_samples) {
    float tx_rms = compute_rms(tx_samples);
    float rx_rms = compute_rms(rx_samples);

    bool tx_active = tx_rms > energy_threshold_;
    bool rx_active = rx_rms > energy_threshold_;

    total_frames_.fetch_add(1, std::memory_order_relaxed);

    if (tx_active && !rx_active) {
        tx_frames_.fetch_add(1, std::memory_order_relaxed);
    } else if (rx_active && !tx_active) {
        rx_frames_.fetch_add(1, std::memory_order_relaxed);
    } else if (tx_active && rx_active) {
        // Both speaking — count as crosstalk, split between both
        tx_frames_.fetch_add(1, std::memory_order_relaxed);
        rx_frames_.fetch_add(1, std::memory_order_relaxed);
    } else {
        silence_frames_.fetch_add(1, std::memory_order_relaxed);
    }
}

TalkTimeStats EnergyDetector::get_stats() const {
    TalkTimeStats stats;

    uint64_t total = total_frames_.load(std::memory_order_relaxed);
    if (total == 0) return stats;

    uint64_t tx = tx_frames_.load(std::memory_order_relaxed);
    uint64_t rx = rx_frames_.load(std::memory_order_relaxed);
    uint64_t silence = silence_frames_.load(std::memory_order_relaxed);

    stats.tx_ratio = static_cast<float>(tx) / static_cast<float>(total);
    stats.rx_ratio = static_cast<float>(rx) / static_cast<float>(total);
    stats.silence_ratio = static_cast<float>(silence) / static_cast<float>(total);

    stats.tx_duration_ms = static_cast<uint32_t>(tx * FRAME_DURATION_MS);
    stats.rx_duration_ms = static_cast<uint32_t>(rx * FRAME_DURATION_MS);
    stats.silence_duration_ms = static_cast<uint32_t>(silence * FRAME_DURATION_MS);
    stats.total_duration_ms = static_cast<uint32_t>(total * FRAME_DURATION_MS);

    return stats;
}

void EnergyDetector::reset() {
    tx_frames_.store(0, std::memory_order_relaxed);
    rx_frames_.store(0, std::memory_order_relaxed);
    silence_frames_.store(0, std::memory_order_relaxed);
    total_frames_.store(0, std::memory_order_relaxed);
}

float EnergyDetector::compute_rms(const std::vector<int16_t>& samples) const {
    if (samples.empty()) return 0.0f;

    double sum_sq = 0.0;
    for (int16_t s : samples) {
        double val = static_cast<double>(s);
        sum_sq += val * val;
    }
    return static_cast<float>(std::sqrt(sum_sq / samples.size()));
}

}  // namespace orbit::vad
