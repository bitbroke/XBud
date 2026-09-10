/**
 * @file whisper_engine.h
 * @brief IndicWhisper ASR engine wrapper using whisper.cpp.
 *
 * Processes 3-second audio segments with the "Local Agreement Policy":
 * segments are flushed on punctuation to prevent exponential CPU growth.
 * Uses ARM NEON SIMD and INT8 quantization for real-time performance.
 */
#pragma once

#include "core/config.h"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

// Forward declarations (whisper.cpp types)
struct whisper_context;

namespace orbit::asr {

enum class Speaker : uint8_t {
    USER = 0,    // TX channel (microphone)
    CALLER = 1,  // RX channel (remote party)
    SYSTEM = 2,  // System-generated text
};

const char* speaker_to_string(Speaker s);

struct TranscriptSegment {
    std::string text;
    Speaker speaker;
    int64_t start_ms = 0;
    int64_t end_ms = 0;
    float confidence = 0.0f;
};

class WhisperEngine {
public:
    explicit WhisperEngine(const core::Config& config);
    ~WhisperEngine();

    /**
     * @brief Load the whisper.cpp model.
     * @return true on success.
     */
    bool initialize();

    /**
     * @brief Transcribe a mono audio segment.
     * @param samples 16-bit PCM mono samples at 16kHz.
     * @param speaker Which channel this audio came from.
     * @return Transcribed text (may be empty if no speech detected).
     */
    std::string transcribe_segment(const std::vector<int16_t>& samples,
                                    Speaker speaker);

    /**
     * @brief Flush accumulated audio and return any remaining text.
     *        Called at call end to process the last partial segment.
     */
    std::string flush();

    /**
     * @brief Reset the engine state (call boundaries).
     */
    void reset();

    /**
     * @brief Get cumulative transcription statistics.
     */
    uint32_t segments_processed() const { return segments_processed_; }
    float average_latency_ms() const;

    bool is_ready() const { return ready_; }

private:
    /**
     * @brief Internal segment-level inference.
     */
    std::string run_inference(const std::vector<float>& audio_f32);

    /**
     * @brief Convert int16 PCM to float32 normalized [-1, 1].
     */
    static std::vector<float> pcm_to_float(const std::vector<int16_t>& pcm);

    std::string model_path_;
    std::string language_;
    int num_threads_ = 2;
    int segment_duration_ms_ = 3000;

    whisper_context* ctx_ = nullptr;
    bool ready_ = false;

    // Accumulation buffer for the segment approach
    std::vector<int16_t> accumulation_buffer_;
    int samples_per_segment_ = 0;

    // Statistics
    uint32_t segments_processed_ = 0;
    double total_latency_ms_ = 0.0;
};

}  // namespace orbit::asr
