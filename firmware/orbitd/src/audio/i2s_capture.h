/**
 * @file i2s_capture.h
 * @brief I2S audio capture from the Bluetooth coprocessor via ALSA.
 *
 * The BES2600 Bluetooth chip routes HFP SCO audio over the I2S bus to
 * the RK3576 SoC. This module captures the 2-channel (TX+RX) PCM stream
 * at 16kHz/16-bit, providing natural hardware diarization.
 */
#pragma once

#include "core/config.h"
#include "audio/ring_buffer.h"
#include "audio/audio_types.h"

#include <string>
#include <vector>
#include <atomic>

// Forward declaration (ALSA types)
struct _snd_pcm;
typedef struct _snd_pcm snd_pcm_t;

namespace orbit::audio {

class I2SCapture {
public:
    I2SCapture(const core::Config& config, RingBuffer& ring_buffer);
    ~I2SCapture();

    /**
     * @brief Initialize ALSA capture device for I2S input.
     * @return true on success.
     */
    bool initialize();

    /**
     * @brief Read one frame of PCM samples (10ms = 160 stereo samples).
     * @return Interleaved stereo samples, or empty if no data available.
     */
    std::vector<int16_t> read_frames();

    /**
     * @brief Check if the I2S stream is active (SCO link is up).
     */
    bool is_active() const;

    /**
     * @brief Shutdown and release ALSA resources.
     */
    void shutdown();

    /**
     * @brief Get the number of frames captured since start.
     */
    uint64_t frames_captured() const;

    /**
     * @brief Get the number of xruns (buffer overruns/underruns).
     */
    uint32_t xrun_count() const;

private:
    const core::Config& config_;
    RingBuffer& ring_buffer_;

    snd_pcm_t* capture_handle_ = nullptr;
    std::string device_name_;
    int period_size_ = SAMPLES_PER_FRAME;

    std::atomic<bool> active_{false};
    std::atomic<uint64_t> frames_captured_{0};
    std::atomic<uint32_t> xrun_count_{0};

    std::vector<int16_t> frame_buffer_;
};

}  // namespace orbit::audio
