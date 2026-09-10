/**
 * @file pii_scrubber.h
 * @brief Hybrid PII scrubbing pipeline: regex pre-filter + DistilBERT NER.
 *
 * Two-stage approach:
 *   1. Regex prefilter (~0.1ms): catches known patterns (Aadhaar, PAN, etc.)
 *   2. DistilBERT NER (~10-30ms): catches context-dependent entities (names, addresses)
 *
 * This ensures no PII reaches the SLM or persistent storage, as required
 * by the DPDP Act 2023.
 */
#pragma once

#include "core/config.h"
#include "ner/regex_prefilter.h"
#include <string>
#include <memory>
#include <vector>

namespace Ort { class Session; class Env; }

namespace orbit::ner {

class PIIScrubber {
public:
    explicit PIIScrubber(const core::Config& config);
    ~PIIScrubber();

    /**
     * @brief Load the DistilBERT NER ONNX model.
     */
    bool initialize();

    /**
     * @brief Scrub PII from text using the hybrid pipeline.
     * @param text Raw transcript text.
     * @return Sanitized text with PII replaced by generic tokens.
     */
    std::string scrub(const std::string& text);

    /**
     * @brief Get statistics on entities scrubbed.
     */
    uint32_t entities_scrubbed() const { return entities_scrubbed_; }

    bool is_ready() const { return ready_; }

private:
    /**
     * @brief Run DistilBERT NER inference on text.
     */
    std::string run_ner(const std::string& text);

    /**
     * @brief Simple whitespace tokenizer for NER input.
     */
    std::vector<std::string> tokenize(const std::string& text) const;

    std::string model_path_;
    RegexPrefilter regex_filter_;

    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;

    bool ready_ = false;
    uint32_t entities_scrubbed_ = 0;

    // NER label mapping
    static const std::vector<std::string> LABEL_MAP;
};

}  // namespace orbit::ner
