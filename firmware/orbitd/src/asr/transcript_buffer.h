/**
 * @file transcript_buffer.h
 * @brief Rolling transcript accumulator with trigger detection.
 *
 * Accumulates sanitized text from ASR and provides it to the SLM
 * engine in configurable windows (30s default). Also detects
 * wake-word triggers and bookmark events for on-demand inference.
 */
#pragma once

#include "core/config.h"
#include "asr/whisper_engine.h"  // for Speaker enum

#include <string>
#include <vector>
#include <mutex>
#include <deque>
#include <chrono>

namespace orbit::asr {

struct TranscriptEntry {
    std::string text;
    Speaker speaker;
    std::chrono::steady_clock::time_point timestamp;
    bool bookmarked = false;
};

class TranscriptBuffer {
public:
    explicit TranscriptBuffer(const core::Config& config);
    ~TranscriptBuffer() = default;

    /**
     * @brief Append a sanitized text segment to the buffer.
     */
    void append(const std::string& text, Speaker speaker);

    /**
     * @brief Mark the last N seconds of transcript as bookmarked.
     * @param seconds Number of seconds to look back (default 30).
     */
    void bookmark(int seconds = 30);

    /**
     * @brief Set a trigger flag (e.g., wake-word detected).
     */
    void set_trigger();

    /**
     * @brief Check if a trigger is pending.
     */
    bool has_trigger() const;

    /**
     * @brief Get recent transcript (since last clear) and reset.
     *        Used by the SLM for periodic batched inference.
     */
    std::string get_recent_and_clear();

    /**
     * @brief Get the full transcript for the entire call.
     *        Used for post-call summary.
     */
    std::string get_full() const;

    /**
     * @brief Get all bookmarked segments.
     */
    std::vector<TranscriptEntry> get_bookmarked() const;

    /**
     * @brief Clear all buffers (call end).
     */
    void clear();

    /**
     * @brief Get total number of entries.
     */
    size_t size() const;

private:
    mutable std::mutex mutex_;
    std::deque<TranscriptEntry> entries_;
    std::deque<TranscriptEntry> recent_entries_;  // Since last SLM batch
    bool trigger_pending_ = false;

    // Configuration
    int max_entries_ = 10000;  // Prevent unbounded growth
    std::vector<std::string> wake_words_;
};

}  // namespace orbit::asr
