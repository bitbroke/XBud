/**
 * @file consent_chime.cpp
 * @brief Consent chime loader and audio injection.
 */

#include "audio/consent_chime.h"
#include <fstream>
#include <iostream>

namespace orbit::audio {

ConsentChime::ConsentChime(const core::Config& config) {
    chime_path_ = config.get_string("consent.chime_path",
                                     "/opt/orbitd/assets/consent_chime.raw");
    sample_rate_ = config.get_int("audio.sample_rate", 16000);
}

bool ConsentChime::load() {
    std::ifstream file(chime_path_, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[consent_chime] Cannot open chime file: " << chime_path_
                  << std::endl;
        return false;
    }

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    size_t num_samples = static_cast<size_t>(size) / sizeof(int16_t);
    samples_.resize(num_samples);

    file.read(reinterpret_cast<char*>(samples_.data()), size);

    if (!file) {
        std::cerr << "[consent_chime] Failed to read chime data" << std::endl;
        samples_.clear();
        return false;
    }

    std::cout << "[consent_chime] Loaded " << num_samples << " samples ("
              << duration_ms() << " ms)" << std::endl;
    return true;
}

int ConsentChime::duration_ms() const {
    if (samples_.empty() || sample_rate_ == 0) return 0;
    return static_cast<int>(samples_.size() * 1000 / sample_rate_);
}

}  // namespace orbit::audio
