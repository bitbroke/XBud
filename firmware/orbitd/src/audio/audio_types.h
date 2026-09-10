/**
 * @file audio_types.h
 * @brief Audio data types and constants used across the pipeline.
 */
#pragma once

#include <cstdint>
#include <vector>
#include <chrono>

namespace orbit::audio {

// ---------------------------------------------------------------------------
// Audio constants
// ---------------------------------------------------------------------------
constexpr int SAMPLE_RATE = 16000;          // 16 kHz (telephony standard)
constexpr int CHANNELS = 2;                 // Stereo: TX (user) + RX (caller)
constexpr int BITS_PER_SAMPLE = 16;         // 16-bit signed PCM
constexpr int FRAME_DURATION_MS = 10;       // 10ms capture cycle
constexpr int SAMPLES_PER_FRAME = SAMPLE_RATE * FRAME_DURATION_MS / 1000;  // 160
constexpr int BYTES_PER_SAMPLE = BITS_PER_SAMPLE / 8;                       // 2

// Ring buffer sizing
constexpr int DEFAULT_BUFFER_SECONDS = 60;
constexpr size_t BYTES_PER_SECOND = SAMPLE_RATE * CHANNELS * BYTES_PER_SAMPLE;  // 64,000

// ---------------------------------------------------------------------------
// PCM frame type
// ---------------------------------------------------------------------------
struct PCMFrame {
    std::vector<int16_t> samples;   // Interleaved stereo: [L, R, L, R, ...]
    std::chrono::steady_clock::time_point timestamp;
    uint32_t sequence_number = 0;

    size_t channel_samples() const { return samples.size() / CHANNELS; }
};

// ---------------------------------------------------------------------------
// Channel identifier
// ---------------------------------------------------------------------------
enum class Channel : uint8_t {
    TX = 0,   // User's microphone (transmitted audio)
    RX = 1,   // Caller's audio (received audio)
};

}  // namespace orbit::audio
