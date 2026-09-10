/**
 * @file energy_detector.h
 * @brief Dual-channel energy ratio detector for talk-time analytics.
 *
 * Computes TX (user) vs RX (caller) energy ratios directly from PCM data.
 * Zero model cost — pure DSP math. Provides real-time talk-time statistics
 * pushed to the companion app via BLE.
 */
#pragma once

#include <vector>
#include <cstdint>
#include <atomic>
#include <chrono>

namespace orbit::vad {

struct TalkTimeStats {
    float tx_ratio = 0.0f;     // User's talk ratio [0, 1]
    float rx_ratio = 0.0f;     // Caller's talk ratio [0, 1]
    float silence_ratio = 0.0f; // Neither speaking [0, 1]

    uint32_t tx_duration_ms = 0;
    uint32_t rx_duration_ms = 0;
    uint32_t silence_duration_ms = 0;
    uint32_t total_duration_ms = 0;
};

class EnergyDetector {
public:
    EnergyDetector() = default;
    ~EnergyDetector() = default;

    /**
     * @brief Process a frame of TX and RX audio to update energy stats.
     * @param tx_samples User microphone audio (mono).
     * @param rx_samples Caller audio (mono).
     */
    void process(const std::vector<int16_t>& tx_samples,
                 const std::vector<int16_t>& rx_samples);

    /**
     * @brief Get current talk-time statistics.
     */
    TalkTimeStats get_stats() const;

    /**
     * @brief Reset all counters (call this at call start).
     */
    void reset();

    /**
     * @brief Set the energy threshold for speech detection (RMS).
     */
    void set_threshold(float threshold) { energy_threshold_ = threshold; }

private:
    /**
     * @brief Compute RMS energy of a PCM buffer.
     */
    float compute_rms(const std::vector<int16_t>& samples) const;

    float energy_threshold_ = 200.0f;  // RMS threshold for "speech"

    std::atomic<uint64_t> tx_frames_{0};
    std::atomic<uint64_t> rx_frames_{0};
    std::atomic<uint64_t> silence_frames_{0};
    std::atomic<uint64_t> total_frames_{0};

    static constexpr int FRAME_DURATION_MS = 30;  // Matches VAD chunk size
};

}  // namespace orbit::vad
