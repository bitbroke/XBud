/**
 * @file ring_buffer.h
 * @brief Lock-free single-producer single-consumer (SPSC) ring buffer for
 *        volatile audio storage.
 *
 * This buffer lives in tmpfs (/volatile/audio_ring) and is mlock()'d to
 * prevent paging to swap. It is securely wiped on call end.
 *
 * Design:
 *   - Lock-free for real-time audio thread safety
 *   - Fixed-size, pre-allocated (no dynamic allocation during calls)
 *   - Overwrite-on-full (oldest data is silently dropped)
 */
#pragma once

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <vector>

namespace orbit::audio {

class RingBuffer {
public:
    /**
     * @brief Construct a ring buffer.
     * @param duration_seconds Duration of audio to buffer (default 60s).
     * @param sample_rate Audio sample rate (default 16000 Hz).
     * @param channels Number of audio channels (default 2).
     */
    RingBuffer(int duration_seconds = 60, int sample_rate = 16000, int channels = 2);
    ~RingBuffer();

    // Non-copyable
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    /**
     * @brief Initialize the buffer: allocate memory, mlock pages.
     * @return true on success.
     */
    bool initialize();

    /**
     * @brief Write interleaved PCM samples into the buffer.
     *        Called by the audio capture thread (producer).
     * @param samples Interleaved stereo samples.
     * @return Number of samples actually written.
     */
    size_t write(const std::vector<int16_t>& samples);

    /**
     * @brief Read interleaved PCM samples from the buffer.
     *        Called by the VAD/ASR thread (consumer).
     * @param out Output buffer, resized to available samples.
     * @return true if samples were read, false if buffer is empty.
     */
    bool read(std::vector<int16_t>& out);

    /**
     * @brief Get the number of samples available for reading.
     */
    size_t available() const;

    /**
     * @brief Check if the buffer is empty.
     */
    bool empty() const;

    /**
     * @brief Securely wipe the buffer contents (memset_s + madvise).
     *        Called during CLEANUP state after call ends.
     */
    void secure_wipe();

    /**
     * @brief Get total capacity in samples.
     */
    size_t capacity() const { return capacity_; }

    /**
     * @brief Get total capacity in bytes.
     */
    size_t capacity_bytes() const { return capacity_ * sizeof(int16_t); }

private:
    int16_t* buffer_ = nullptr;      // Raw buffer (mlock'd)
    size_t capacity_ = 0;            // Total capacity in samples

    // Lock-free SPSC indices
    alignas(64) std::atomic<size_t> write_pos_{0};
    alignas(64) std::atomic<size_t> read_pos_{0};

    int duration_seconds_;
    int sample_rate_;
    int channels_;
    bool mlocked_ = false;
};

}  // namespace orbit::audio
