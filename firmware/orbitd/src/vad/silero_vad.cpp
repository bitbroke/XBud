/**
 * @file silero_vad.cpp
 * @brief Silero VAD ONNX Runtime inference implementation.
 */

#include "vad/silero_vad.h"
#include <onnxruntime_cxx_api.h>
#include <iostream>
#include <algorithm>
#include <cmath>

namespace orbit::vad {

SileroVAD::SileroVAD(const core::Config& config) {
    model_path_ = config.get_string("vad.model_path",
                                     "/opt/orbitd/models/silero-vad-v5.onnx");
    threshold_ = config.get_float("vad.threshold", 0.5f);
    sample_rate_ = config.get_int("audio.sample_rate", 16000);
}

SileroVAD::~SileroVAD() = default;

bool SileroVAD::initialize() {
    try {
        env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "silero_vad");

        Ort::SessionOptions session_opts;
        session_opts.SetIntraOpNumThreads(1);
        session_opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        session_ = std::make_unique<Ort::Session>(*env_, model_path_.c_str(), session_opts);

        // Initialize LSTM hidden/cell states (2 layers, 1 batch, 64 hidden)
        h_state_.resize(2 * 1 * 64, 0.0f);
        c_state_.resize(2 * 1 * 64, 0.0f);

        sr_tensor_[0] = sample_rate_;

        ready_ = true;
        std::cout << "[vad] Silero VAD loaded from " << model_path_ << std::endl;
        return true;

    } catch (const Ort::Exception& e) {
        std::cerr << "[vad] ONNX error: " << e.what() << std::endl;
        return false;
    }
}

float SileroVAD::process(const std::vector<int16_t>& samples) {
    if (!ready_ || !session_) return 0.0f;

    try {
        auto memory_info = Ort::MemoryInfo::CreateCpu(
            OrtArenaAllocator, OrtMemTypeDefault);

        // Convert int16 to float32 and normalize
        std::vector<float> input_data(samples.size());
        for (size_t i = 0; i < samples.size(); ++i) {
            input_data[i] = static_cast<float>(samples[i]) / 32768.0f;
        }

        // Input tensors
        int64_t input_shape[] = {1, static_cast<int64_t>(input_data.size())};
        int64_t h_shape[] = {2, 1, 64};
        int64_t c_shape[] = {2, 1, 64};
        int64_t sr_shape[] = {1};

        std::vector<Ort::Value> input_tensors;
        input_tensors.push_back(Ort::Value::CreateTensor<float>(
            memory_info, input_data.data(), input_data.size(), input_shape, 2));
        input_tensors.push_back(Ort::Value::CreateTensor<int64_t>(
            memory_info, sr_tensor_, 1, sr_shape, 1));
        input_tensors.push_back(Ort::Value::CreateTensor<float>(
            memory_info, h_state_.data(), h_state_.size(), h_shape, 3));
        input_tensors.push_back(Ort::Value::CreateTensor<float>(
            memory_info, c_state_.data(), c_state_.size(), c_shape, 3));

        // Input/output names
        const char* input_names[] = {"input", "sr", "h", "c"};
        const char* output_names[] = {"output", "hn", "cn"};

        // Run inference
        auto output_tensors = session_->Run(
            Ort::RunOptions{nullptr},
            input_names, input_tensors.data(), input_tensors.size(),
            output_names, 3
        );

        // Extract speech probability
        float speech_prob = output_tensors[0].GetTensorData<float>()[0];

        // Update LSTM states for next call
        const float* hn = output_tensors[1].GetTensorData<float>();
        const float* cn = output_tensors[2].GetTensorData<float>();
        std::copy(hn, hn + h_state_.size(), h_state_.begin());
        std::copy(cn, cn + c_state_.size(), c_state_.begin());

        return speech_prob;

    } catch (const Ort::Exception& e) {
        std::cerr << "[vad] Inference error: " << e.what() << std::endl;
        return 0.0f;
    }
}

void SileroVAD::reset() {
    std::fill(h_state_.begin(), h_state_.end(), 0.0f);
    std::fill(c_state_.begin(), c_state_.end(), 0.0f);
}

}  // namespace orbit::vad
