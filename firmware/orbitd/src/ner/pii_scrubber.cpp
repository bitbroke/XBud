/**
 * @file pii_scrubber.cpp
 * @brief Hybrid PII scrubbing pipeline implementation.
 */

#include "ner/pii_scrubber.h"
#include <onnxruntime_cxx_api.h>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace orbit::ner {

// BIO label mapping for the fine-tuned DistilBERT NER model
const std::vector<std::string> PIIScrubber::LABEL_MAP = {
    "O",            // 0: Outside any entity
    "B-PERSON",     // 1: Beginning of person name
    "I-PERSON",     // 2: Inside person name
    "B-ADDRESS",    // 3: Beginning of address
    "I-ADDRESS",    // 4: Inside address
    "B-GOV_ID",     // 5: Beginning of government ID
    "I-GOV_ID",     // 6: Inside government ID
    "B-CARD",       // 7: Beginning of card number
    "I-CARD",       // 8: Inside card number
    "B-PHONE",      // 9: Beginning of phone number
    "I-PHONE",      // 10: Inside phone number
    "B-ORG",        // 11: Beginning of organization
    "I-ORG",        // 12: Inside organization
};

PIIScrubber::PIIScrubber(const core::Config& config) {
    model_path_ = config.get_string("ner.model_path",
                                     "/opt/orbitd/models/distilbert-ner-int8.onnx");
}

PIIScrubber::~PIIScrubber() = default;

bool PIIScrubber::initialize() {
    try {
        env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "ner_scrubber");

        Ort::SessionOptions session_opts;
        session_opts.SetIntraOpNumThreads(1);
        session_opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        session_ = std::make_unique<Ort::Session>(*env_, model_path_.c_str(), session_opts);

        ready_ = true;
        std::cout << "[ner] DistilBERT NER loaded from " << model_path_ << std::endl;
        return true;

    } catch (const Ort::Exception& e) {
        std::cerr << "[ner] ONNX error: " << e.what() << std::endl;
        return false;
    }
}

std::string PIIScrubber::scrub(const std::string& text) {
    if (text.empty()) return text;

    // Stage 1: Fast regex pre-filter
    std::string filtered = regex_filter_.filter(text);

    // Stage 2: Neural NER for context-dependent entities
    if (ready_) {
        filtered = run_ner(filtered);
    }

    return filtered;
}

std::string PIIScrubber::run_ner(const std::string& text) {
    if (!session_) return text;

    auto tokens = tokenize(text);
    if (tokens.empty()) return text;

    try {
        auto memory_info = Ort::MemoryInfo::CreateCpu(
            OrtArenaAllocator, OrtMemTypeDefault);

        // Simple word-level tokenization → token IDs
        // In production, this would use a proper WordPiece tokenizer
        // For now, we use word indices as a placeholder
        std::vector<int64_t> input_ids(tokens.size());
        std::vector<int64_t> attention_mask(tokens.size(), 1);

        // Token ID generation (simplified — production uses WordPiece)
        for (size_t i = 0; i < tokens.size(); ++i) {
            // Hash-based token ID (placeholder for proper tokenizer)
            uint64_t hash = 0;
            for (char c : tokens[i]) {
                hash = hash * 31 + static_cast<unsigned char>(c);
            }
            input_ids[i] = static_cast<int64_t>(hash % 30000) + 1;
        }

        int64_t seq_len = static_cast<int64_t>(tokens.size());
        int64_t input_shape[] = {1, seq_len};

        std::vector<Ort::Value> input_tensors;
        input_tensors.push_back(Ort::Value::CreateTensor<int64_t>(
            memory_info, input_ids.data(), input_ids.size(), input_shape, 2));
        input_tensors.push_back(Ort::Value::CreateTensor<int64_t>(
            memory_info, attention_mask.data(), attention_mask.size(), input_shape, 2));

        const char* input_names[] = {"input_ids", "attention_mask"};
        const char* output_names[] = {"logits"};

        auto output_tensors = session_->Run(
            Ort::RunOptions{nullptr},
            input_names, input_tensors.data(), input_tensors.size(),
            output_names, 1
        );

        // Parse logits to get predicted labels
        const float* logits = output_tensors[0].GetTensorData<float>();
        auto shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();
        int num_labels = static_cast<int>(shape[2]);

        std::ostringstream result;
        for (size_t t = 0; t < tokens.size(); ++t) {
            // Argmax over label dimension
            int best_label = 0;
            float best_score = logits[t * num_labels];
            for (int l = 1; l < num_labels && l < static_cast<int>(LABEL_MAP.size()); ++l) {
                if (logits[t * num_labels + l] > best_score) {
                    best_score = logits[t * num_labels + l];
                    best_label = l;
                }
            }

            const std::string& label = LABEL_MAP[best_label];

            if (label == "O") {
                result << tokens[t];
            } else if (label.find("PERSON") != std::string::npos) {
                result << "[NAME]";
                entities_scrubbed_++;
            } else if (label.find("ADDRESS") != std::string::npos) {
                result << "[ADDRESS]";
                entities_scrubbed_++;
            } else if (label.find("GOV_ID") != std::string::npos) {
                result << "[GOV_ID]";
                entities_scrubbed_++;
            } else if (label.find("CARD") != std::string::npos) {
                result << "[CARD]";
                entities_scrubbed_++;
            } else if (label.find("PHONE") != std::string::npos) {
                result << "[PHONE]";
                entities_scrubbed_++;
            } else if (label.find("ORG") != std::string::npos) {
                // Organizations are NOT scrubbed (not PII)
                result << tokens[t];
            } else {
                result << tokens[t];
            }

            if (t + 1 < tokens.size()) result << " ";
        }

        return result.str();

    } catch (const Ort::Exception& e) {
        std::cerr << "[ner] Inference error: " << e.what() << std::endl;
        return text;  // Fail-safe: return original text
    }
}

std::vector<std::string> PIIScrubber::tokenize(const std::string& text) const {
    std::vector<std::string> tokens;
    std::istringstream ss(text);
    std::string word;
    while (ss >> word) {
        tokens.push_back(word);
    }
    return tokens;
}

}  // namespace orbit::ner
