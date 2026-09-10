/**
 * @file ring_buffer.cpp
 * @brief Lock-free SPSC ring buffer implementation.
 */

#include "audio/ring_buffer.h"

#include <cstring>
#include <iostream>
#include <new>

#ifdef __linux__
#include <sys/mman.h>  // mlock, munlock, madvise
#endif

namespace orbit::audio {

RingBuffer::RingBuffer(int duration_seconds, int sample_rate, int channels)
    : duration_seconds_(duration_seconds)
    , sample_rate_(sample_rate)
    , channels_(channels)
{
    // Total samples = duration × sample_rate × channels
    // Round up to power of 2 for efficient modulo via bitwise AND
    size_t raw_size = static_cast<size_t>(duration_seconds) *
                      static_cast<size_t>(sample_rate) *
                      static_cast<size_t>(channels);

    // Find next power of 2
    capacity_ = 1;
    while (capacity_ < raw_size) {
        capacity_ <<= 1;
    }
}

RingBuffer::~RingBuffer() {
    secure_wipe();

    if (buffer_) {
#ifdef __linux__
        if (mlocked_) {
            munlock(buffer_, capacity_ * sizeof(int16_t));
        }
#endif
        delete[] buffer_;
        buffer_ = nullptr;
    }
}

bool RingBuffer::initialize() {
    try {
        buffer_ = new int16_t[capacity_]();  // Zero-initialized
    } catch (const std::bad_alloc& e) {
        std::cerr << "[ring_buffer] Failed to allocate "
                  << capacity_bytes() << " bytes: " << e.what() << std::endl;
        return false;
    }

#ifdef __linux__
    // Lock pages into RAM to prevent swapping of sensitive audio data
    if (mlock(buffer_, capacity_ * sizeof(int16_t)) == 0) {
        mlocked_ = true;
        std::cout << "[ring_buffer] mlock'd " << capacity_bytes() / 1024
                  << " KB of audio buffer" << std::endl;
    } else {
        std::cerr << "[ring_buffer] WARNING: mlock failed, audio may be swapped"
                  << std::endl;
    }
#endif

    write_pos_.store(0, std::memory_order_relaxed);
    read_pos_.store(0, std::memory_order_relaxed);

    std::cout << "[ring_buffer] Initialized: " << duration_seconds_ << "s, "
              << capacity_bytes() / 1024 << " KB" << std::endl;
    return true;
}

size_t RingBuffer::write(const std::vector<int16_t>& samples) {
    if (!buffer_ || samples.empty()) return 0;

    const size_t mask = capacity_ - 1;  // Power-of-2 modulo
    size_t wp = write_pos_.load(std::memory_order_relaxed);

    for (size_t i = 0; i < samples.size(); ++i) {
        buffer_[(wp + i) & mask] = samples[i];
    }

    write_pos_.store(wp + samples.size(), std::memory_order_release);
    return samples.size();
}

bool RingBuffer::read(std::vector<int16_t>& out) {
    if (!buffer_) return false;

    const size_t mask = capacity_ - 1;
    size_t rp = read_pos_.load(std::memory_order_relaxed);
    size_t wp = write_pos_.load(std::memory_order_acquire);

    size_t avail = wp - rp;
    if (avail == 0) return false;

    // Limit read to requested size or available
    size_t to_read = std::min(avail, out.size());
    if (to_read == 0) {
        to_read = avail;
        out.resize(to_read);
    }

    for (size_t i = 0; i < to_read; ++i) {
        out[i] = buffer_[(rp + i) & mask];
    }

    read_pos_.store(rp + to_read, std::memory_order_release);
    return true;
}

size_t RingBuffer::available() const {
    size_t wp = write_pos_.load(std::memory_order_acquire);
    size_t rp = read_pos_.load(std::memory_order_acquire);
    return wp - rp;
}

bool RingBuffer::empty() const {
    return available() == 0;
}

void RingBuffer::secure_wipe() {
    if (!buffer_) return;

    // Use volatile to prevent compiler from optimizing away the memset
    volatile int16_t* volatile_buf = buffer_;
    for (size_t i = 0; i < capacity_; ++i) {
        volatile_buf[i] = 0;
    }

#ifdef __linux__
    // Advise kernel to release the physical pages
    madvise(buffer_, capacity_ * sizeof(int16_t), MADV_DONTNEED);
#endif

    write_pos_.store(0, std::memory_order_release);
    read_pos_.store(0, std::memory_order_release);

    std::cout << "[ring_buffer] Secure wipe complete ("
              << capacity_bytes() / 1024 << " KB)" << std::endl;
}

}  // namespace orbit::audio
