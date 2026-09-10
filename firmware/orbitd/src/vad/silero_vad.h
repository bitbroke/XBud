/**
 * @file silero_vad.h
 * @brief Silero VAD v5 wrapper using ONNX Runtime.
 *
 * Processes 30ms audio chunks and returns a speech probability [0, 1].
 * ~2MB model, ~1ms per chunk on ARM CPU. Far superior noise robustness
 * vs WebRTC VAD in Indian traffic/commute environments.
 */
#pragma once

#include "core/config.h"
#include <vector>
#include <memory>
#include <cstdint>

// Forward declarations (ONNX Runtime)
namespace Ort {
    class Session;
    class Env;
    class MemoryInfo;
}

namespace orbit::vad {

class SileroVAD {
public:
    explicit SileroVAD(const core::Config& config);
    ~SileroVAD();

    /**
     * @brief Load the Silero VAD ONNX model.
     * @return true on success.
     */
    bool initialize();

    /**
     * @brief Process a 30ms audio chunk and return speech probability.
     * @param samples Mono 16-bit PCM samples (480 samples at 16kHz).
     * @return Speech probability [0.0, 1.0].
     */
    float process(const std::vector<int16_t>& samples);

    /**
     * @brief Reset the internal state (call this between calls).
     */
    void reset();

    /**
     * @brief Check if the model is loaded and ready.
     */
    bool is_ready() const { return ready_; }

    /**
     * @brief Get/set the speech threshold (default 0.5).
     */
    float threshold() const { return threshold_; }
    void set_threshold(float t) { threshold_ = t; }

private:
    std::string model_path_;
    float threshold_ = 0.5f;
    int sample_rate_ = 16000;
    bool ready_ = false;

    // ONNX Runtime session
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;

    // Internal state tensors (Silero VAD is stateful — LSTM hidden/cell)
    std::vector<float> h_state_;
    std::vector<float> c_state_;
    int64_t sr_tensor_[1] = {16000};
};

}  // namespace orbit::vad
