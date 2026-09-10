/**
 * @file consent_chime.h
 * @brief Audio consent announcement injected into call stream for DPDP compliance.
 */
#pragma once

#include "core/config.h"
#include <string>
#include <vector>
#include <cstdint>

namespace orbit::audio {

class ConsentChime {
public:
    explicit ConsentChime(const core::Config& config);
    ~ConsentChime() = default;

    /**
     * @brief Load the consent chime PCM audio from file.
     * @return true on success.
     */
    bool load();

    /**
     * @brief Get the raw PCM samples of the consent chime.
     *        These should be mixed into the TX (outgoing) audio stream.
     */
    const std::vector<int16_t>& samples() const { return samples_; }

    /**
     * @brief Get the duration of the chime in milliseconds.
     */
    int duration_ms() const;

    /**
     * @brief Check if the chime has been loaded.
     */
    bool is_loaded() const { return !samples_.empty(); }

private:
    std::string chime_path_;
    std::vector<int16_t> samples_;
    int sample_rate_ = 16000;
};

}  // namespace orbit::audio
