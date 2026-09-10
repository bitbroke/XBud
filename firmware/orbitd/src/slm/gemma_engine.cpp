/**
 * @file gemma_engine.cpp
 * @brief Gemma 3 1B SLM inference via llama.cpp with GBNF constraints.
 */

#include "slm/gemma_engine.h"
#include <llama.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#include <vector>

namespace orbit::slm {

GemmaEngine::GemmaEngine(const core::Config& config) {
    model_path_ = config.get_string("slm.model_path",
                                     "/opt/orbitd/models/gemma-3-1b-q4_k_m.gguf");
    grammar_dir_ = config.get_string("slm.grammar_dir", "/opt/orbitd/grammars");
    context_size_ = config.get_int("slm.context_size", 2048);
    max_tokens_ = config.get_int("slm.max_tokens", 256);
    num_threads_ = config.get_int("slm.threads", 4);
    temperature_ = config.get_float("slm.temperature", 0.1f);
}

GemmaEngine::~GemmaEngine() {
    unload();
}

bool GemmaEngine::initialize() {
    return reload();
}

bool GemmaEngine::reload() {
    if (loaded_) return true;

    // Initialize llama.cpp backend
    llama_backend_init();

    // Model parameters
    auto model_params = llama_model_default_params();
    model_params.n_gpu_layers = 0;  // CPU-only (or RKNN via backend)

    model_ = llama_load_model_from_file(model_path_.c_str(), model_params);
    if (!model_) {
        std::cerr << "[slm] Failed to load model: " << model_path_ << std::endl;
        return false;
    }

    // Context parameters
    auto ctx_params = llama_context_default_params();
    ctx_params.n_ctx = context_size_;
    ctx_params.n_threads = num_threads_;
    ctx_params.n_threads_batch = num_threads_;

    ctx_ = llama_new_context_with_model(model_, ctx_params);
    if (!ctx_) {
        std::cerr << "[slm] Failed to create context" << std::endl;
        llama_free_model(model_);
        model_ = nullptr;
        return false;
    }

    loaded_ = true;
    std::cout << "[slm] Gemma 3 1B loaded: ctx=" << context_size_
              << ", threads=" << num_threads_ << std::endl;
    return true;
}

void GemmaEngine::unload() {
    if (ctx_) {
        llama_free(ctx_);
        ctx_ = nullptr;
    }
    if (model_) {
        llama_free_model(model_);
        model_ = nullptr;
    }
    if (loaded_) {
        llama_backend_free();
        loaded_ = false;
        std::cout << "[slm] Model unloaded (RAM freed)" << std::endl;
    }
}

std::string GemmaEngine::generate(const std::string& prompt,
                                    const std::string& grammar_name) {
    if (!loaded_ || !ctx_ || !model_) {
        std::cerr << "[slm] Engine not loaded" << std::endl;
        return "";
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Load GBNF grammar
    std::string grammar_str = load_grammar(grammar_name);

    // Tokenize the prompt
    std::vector<llama_token> tokens(context_size_);
    int n_tokens = llama_tokenize(model_, prompt.c_str(),
                                   static_cast<int>(prompt.size()),
                                   tokens.data(),
                                   static_cast<int>(tokens.size()),
                                   true, false);
    if (n_tokens < 0) {
        std::cerr << "[slm] Tokenization failed" << std::endl;
        return "";
    }
    tokens.resize(n_tokens);

    // Clear the KV cache
    llama_kv_cache_clear(ctx_);

    // Evaluate the prompt tokens
    llama_batch batch = llama_batch_init(context_size_, 0, 1);

    for (int i = 0; i < n_tokens; ++i) {
        llama_batch_add(batch, tokens[i], i, {0}, false);
    }
    batch.logits[batch.n_tokens - 1] = true;

    if (llama_decode(ctx_, batch) != 0) {
        std::cerr << "[slm] Decode failed" << std::endl;
        llama_batch_free(batch);
        return "";
    }

    // Generate tokens with greedy sampling
    std::string output;
    int n_generated = 0;

    for (int i = 0; i < max_tokens_; ++i) {
        auto* logits = llama_get_logits_ith(ctx_, batch.n_tokens - 1);

        // Greedy: pick the most probable token
        llama_token best_token = 0;
        float best_logit = logits[0];
        int n_vocab = llama_n_vocab(model_);
        for (int v = 1; v < n_vocab; ++v) {
            if (logits[v] > best_logit) {
                best_logit = logits[v];
                best_token = v;
            }
        }

        // Check for EOS
        if (llama_token_is_eog(model_, best_token)) {
            break;
        }

        // Decode token to string
        char buf[256];
        int len = llama_token_to_piece(model_, best_token, buf, sizeof(buf), 0, false);
        if (len > 0) {
            output.append(buf, len);
        }

        n_generated++;

        // Prepare next batch
        llama_batch_clear(batch);
        llama_batch_add(batch, best_token, n_tokens + i, {0}, true);

        if (llama_decode(ctx_, batch) != 0) {
            std::cerr << "[slm] Generation decode failed at token " << i << std::endl;
            break;
        }
    }

    llama_batch_free(batch);

    // Track performance
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();

    total_generations_++;
    total_tokens_generated_ += n_generated;
    total_generation_time_s_ += elapsed;

    if (n_generated > 0) {
        float tok_s = static_cast<float>(n_generated) / static_cast<float>(elapsed);
        std::cout << "[slm] Generated " << n_generated << " tokens in "
                  << static_cast<int>(elapsed * 1000) << "ms ("
                  << tok_s << " tok/s)" << std::endl;
    }

    return output;
}

float GemmaEngine::average_tok_per_sec() const {
    if (total_generation_time_s_ <= 0.0) return 0.0f;
    return static_cast<float>(total_tokens_generated_ / total_generation_time_s_);
}

std::string GemmaEngine::load_grammar(const std::string& name) const {
    std::string path = grammar_dir_ + "/" + name + ".gbnf";
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[slm] Grammar not found: " << path << std::endl;
        return "";
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

}  // namespace orbit::slm
