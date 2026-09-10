/**
 * @file whisper_engine.cpp
 * @brief IndicWhisper ASR engine implementation using whisper.cpp.
 */

#include "asr/whisper_engine.h"
#include <whisper.h>
#include <iostream>
#include <chrono>
#include <cstring>

namespace orbit::asr {

const char* speaker_to_string(Speaker s) {
    switch (s) {
        case Speaker::USER:   return "USER";
        case Speaker::CALLER: return "CALLER";
        case Speaker::SYSTEM: return "SYSTEM";
        default:              return "UNKNOWN";
    }
}

WhisperEngine::WhisperEngine(const core::Config& config) {
    model_path_ = config.get_string("asr.model_path",
                                     "/opt/orbitd/models/whisper-base-indic-q8_0.bin");
    language_ = config.get_string("asr.language", "hi");
    num_threads_ = config.get_int("asr.threads", 2);
    segment_duration_ms_ = config.get_int("asr.segment_duration_ms", 3000);

    // Calculate samples per segment
    int sample_rate = config.get_int("audio.sample_rate", 16000);
    samples_per_segment_ = (sample_rate * segment_duration_ms_) / 1000;
}

WhisperEngine::~WhisperEngine() {
    if (ctx_) {
        whisper_free(ctx_);
        ctx_ = nullptr;
    }
}

bool WhisperEngine::initialize() {
    struct whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = false;  // CPU-only on embedded ARM

    ctx_ = whisper_init_from_file_with_params(model_path_.c_str(), cparams);
    if (!ctx_) {
        std::cerr << "[asr] Failed to load model: " << model_path_ << std::endl;
        return false;
    }

    ready_ = true;
    std::cout << "[asr] IndicWhisper loaded: " << model_path_
              << " (threads=" << num_threads_
              << ", segment=" << segment_duration_ms_ << "ms)" << std::endl;
    return true;
}

std::string WhisperEngine::transcribe_segment(
    const std::vector<int16_t>& samples, Speaker speaker)
{
    if (!ready_) return "";

    // Accumulate samples until we reach segment_duration_ms
    accumulation_buffer_.insert(accumulation_buffer_.end(),
                                 samples.begin(), samples.end());

    // Check if we have enough samples for a segment
    if (static_cast<int>(accumulation_buffer_.size()) < samples_per_segment_) {
        return "";
    }

    // Extract one segment
    std::vector<int16_t> segment(
        accumulation_buffer_.begin(),
        accumulation_buffer_.begin() + samples_per_segment_
    );
    accumulation_buffer_.erase(
        accumulation_buffer_.begin(),
        accumulation_buffer_.begin() + samples_per_segment_
    );

    // Convert to float and run inference
    auto audio_f32 = pcm_to_float(segment);
    auto start = std::chrono::high_resolution_clock::now();

    std::string text = run_inference(audio_f32);

    auto end = std::chrono::high_resolution_clock::now();
    double latency = std::chrono::duration<double, std::milli>(end - start).count();

    segments_processed_++;
    total_latency_ms_ += latency;

    if (!text.empty()) {
        // Prefix with speaker label
        text = "[" + std::string(speaker_to_string(speaker)) + "] " + text;
    }

    return text;
}

std::string WhisperEngine::flush() {
    if (!ready_ || accumulation_buffer_.empty()) return "";

    // Process remaining audio even if shorter than segment duration
    auto audio_f32 = pcm_to_float(accumulation_buffer_);
    accumulation_buffer_.clear();

    return run_inference(audio_f32);
}

void WhisperEngine::reset() {
    accumulation_buffer_.clear();
    segments_processed_ = 0;
    total_latency_ms_ = 0.0;
}

float WhisperEngine::average_latency_ms() const {
    if (segments_processed_ == 0) return 0.0f;
    return static_cast<float>(total_latency_ms_ / segments_processed_);
}

std::string WhisperEngine::run_inference(const std::vector<float>& audio_f32) {
    struct whisper_full_params params = whisper_full_default_params(
        WHISPER_SAMPLING_GREEDY
    );

    params.n_threads = num_threads_;
    params.language = language_.c_str();
    params.translate = false;
    params.no_timestamps = true;
    params.single_segment = true;
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.suppress_blank = true;
    params.suppress_non_speech_tokens = true;

    // Run inference
    int result = whisper_full(ctx_, params, audio_f32.data(),
                              static_cast<int>(audio_f32.size()));
    if (result != 0) {
        std::cerr << "[asr] Inference failed (error=" << result << ")" << std::endl;
        return "";
    }

    // Collect output segments
    std::string output;
    int n_segments = whisper_full_n_segments(ctx_);
    for (int i = 0; i < n_segments; ++i) {
        const char* text = whisper_full_get_segment_text(ctx_, i);
        if (text) {
            output += text;
        }
    }

    // Trim whitespace
    while (!output.empty() && output.front() == ' ') output.erase(output.begin());
    while (!output.empty() && output.back() == ' ') output.pop_back();

    return output;
}

std::vector<float> WhisperEngine::pcm_to_float(const std::vector<int16_t>& pcm) {
    std::vector<float> f32(pcm.size());
    for (size_t i = 0; i < pcm.size(); ++i) {
        f32[i] = static_cast<float>(pcm[i]) / 32768.0f;
    }
    return f32;
}

}  // namespace orbit::asr
