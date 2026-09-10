/**
 * @file gemma_engine.h
 * @brief Gemma 3 1B SLM inference engine using llama.cpp.
 *
 * Runs GBNF-constrained generation to guarantee valid JSON output.
 * Supports RKNN NPU offload via rk-llama.cpp backend.
 */
#pragma once

#include "core/config.h"
#include <string>
#include <memory>

// Forward declarations (llama.cpp types)
struct llama_model;
struct llama_context;

namespace orbit::slm {

class GemmaEngine {
public:
    explicit GemmaEngine(const core::Config& config);
    ~GemmaEngine();

    bool initialize();

    /**
     * @brief Generate structured JSON output from a prompt.
     * @param prompt The instruction prompt.
     * @param grammar_name Name of the GBNF grammar file to use (without extension).
     * @return Generated JSON string (guaranteed valid by GBNF constraint).
     */
    std::string generate(const std::string& prompt, const std::string& grammar_name);

    /**
     * @brief Unload the model to free RAM (thermal emergency).
     */
    void unload();

    /**
     * @brief Reload the model after thermal recovery.
     */
    bool reload();

    bool is_loaded() const { return loaded_; }
    float average_tok_per_sec() const;
    uint32_t total_generations() const { return total_generations_; }

private:
    std::string load_grammar(const std::string& name) const;

    std::string model_path_;
    std::string grammar_dir_;
    int context_size_ = 2048;
    int max_tokens_ = 256;
    int num_threads_ = 4;
    float temperature_ = 0.1f;

    llama_model* model_ = nullptr;
    llama_context* ctx_ = nullptr;
    bool loaded_ = false;

    uint32_t total_generations_ = 0;
    double total_tokens_generated_ = 0.0;
    double total_generation_time_s_ = 0.0;
};

}  // namespace orbit::slm
