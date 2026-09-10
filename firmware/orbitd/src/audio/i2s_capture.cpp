/**
 * @file i2s_capture.cpp
 * @brief ALSA-based I2S audio capture implementation.
 */

#include "audio/i2s_capture.h"
#include <alsa/asoundlib.h>
#include <iostream>
#include <cstring>

namespace orbit::audio {

I2SCapture::I2SCapture(const core::Config& config, RingBuffer& ring_buffer)
    : config_(config)
    , ring_buffer_(ring_buffer)
{
    device_name_ = config.get_string("audio.device", "hw:1,0");
    period_size_ = config.get_int("audio.period_size", SAMPLES_PER_FRAME);
    frame_buffer_.resize(period_size_ * CHANNELS);
}

I2SCapture::~I2SCapture() {
    shutdown();
}

bool I2SCapture::initialize() {
    int err;

    // Open capture device
    err = snd_pcm_open(&capture_handle_, device_name_.c_str(),
                       SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        std::cerr << "[i2s] Failed to open " << device_name_ << ": "
                  << snd_strerror(err) << std::endl;
        return false;
    }

    // Configure hardware parameters
    snd_pcm_hw_params_t* hw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(capture_handle_, hw_params);

    // Set access type: interleaved
    err = snd_pcm_hw_params_set_access(capture_handle_, hw_params,
                                        SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        std::cerr << "[i2s] Cannot set access type: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Set format: signed 16-bit little-endian
    err = snd_pcm_hw_params_set_format(capture_handle_, hw_params,
                                        SND_PCM_FORMAT_S16_LE);
    if (err < 0) {
        std::cerr << "[i2s] Cannot set format: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Set sample rate: 16000 Hz
    unsigned int rate = SAMPLE_RATE;
    err = snd_pcm_hw_params_set_rate_near(capture_handle_, hw_params, &rate, nullptr);
    if (err < 0) {
        std::cerr << "[i2s] Cannot set rate: " << snd_strerror(err) << std::endl;
        return false;
    }
    if (rate != SAMPLE_RATE) {
        std::cerr << "[i2s] WARNING: Requested rate " << SAMPLE_RATE
                  << " Hz, got " << rate << " Hz" << std::endl;
    }

    // Set channels: 2 (TX + RX from I2S)
    err = snd_pcm_hw_params_set_channels(capture_handle_, hw_params, CHANNELS);
    if (err < 0) {
        std::cerr << "[i2s] Cannot set channels: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Set period size (frames per read)
    snd_pcm_uframes_t period = period_size_;
    err = snd_pcm_hw_params_set_period_size_near(capture_handle_, hw_params,
                                                   &period, nullptr);
    if (err < 0) {
        std::cerr << "[i2s] Cannot set period size: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Set buffer size (4 periods for safety)
    snd_pcm_uframes_t buffer_size = period * 4;
    err = snd_pcm_hw_params_set_buffer_size_near(capture_handle_, hw_params,
                                                    &buffer_size);
    if (err < 0) {
        std::cerr << "[i2s] Cannot set buffer size: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Apply hardware parameters
    err = snd_pcm_hw_params(capture_handle_, hw_params);
    if (err < 0) {
        std::cerr << "[i2s] Cannot set hw params: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Prepare the device
    err = snd_pcm_prepare(capture_handle_);
    if (err < 0) {
        std::cerr << "[i2s] Cannot prepare device: " << snd_strerror(err) << std::endl;
        return false;
    }

    active_.store(true, std::memory_order_release);
    std::cout << "[i2s] Capture ready: " << device_name_
              << " @ " << rate << "Hz, " << CHANNELS << "ch, "
              << period << " frames/period" << std::endl;
    return true;
}

std::vector<int16_t> I2SCapture::read_frames() {
    if (!capture_handle_ || !active_.load(std::memory_order_acquire)) {
        return {};
    }

    snd_pcm_sframes_t frames = snd_pcm_readi(
        capture_handle_,
        frame_buffer_.data(),
        period_size_
    );

    if (frames < 0) {
        // Handle xrun (buffer overrun)
        if (frames == -EPIPE) {
            xrun_count_.fetch_add(1, std::memory_order_relaxed);
            std::cerr << "[i2s] XRUN (overrun) — recovering" << std::endl;
            snd_pcm_prepare(capture_handle_);
            return {};
        }

        // Handle other errors
        frames = snd_pcm_recover(capture_handle_, static_cast<int>(frames), 0);
        if (frames < 0) {
            std::cerr << "[i2s] Read error: " << snd_strerror(static_cast<int>(frames))
                      << std::endl;
            return {};
        }
    }

    frames_captured_.fetch_add(static_cast<uint64_t>(frames), std::memory_order_relaxed);

    // Return the captured samples (interleaved stereo)
    size_t sample_count = static_cast<size_t>(frames) * CHANNELS;
    return std::vector<int16_t>(frame_buffer_.begin(),
                                 frame_buffer_.begin() + sample_count);
}

bool I2SCapture::is_active() const {
    return active_.load(std::memory_order_acquire);
}

void I2SCapture::shutdown() {
    active_.store(false, std::memory_order_release);

    if (capture_handle_) {
        snd_pcm_drop(capture_handle_);
        snd_pcm_close(capture_handle_);
        capture_handle_ = nullptr;
        std::cout << "[i2s] Capture device closed" << std::endl;
    }
}

uint64_t I2SCapture::frames_captured() const {
    return frames_captured_.load(std::memory_order_relaxed);
}

uint32_t I2SCapture::xrun_count() const {
    return xrun_count_.load(std::memory_order_relaxed);
}

}  // namespace orbit::audio
