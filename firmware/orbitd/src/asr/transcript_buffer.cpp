/**
 * @file transcript_buffer.cpp
 * @brief Transcript accumulator implementation.
 */

#include "asr/transcript_buffer.h"
#include <algorithm>
#include <sstream>

namespace orbit::asr {

TranscriptBuffer::TranscriptBuffer(const core::Config& config) {
    max_entries_ = config.get_int("transcript.max_entries", 10000);

    // Load wake words from config
    std::string wake_words_str = config.get_string("transcript.wake_words",
                                                     "assistant,note this down,hey orbit");
    std::istringstream ss(wake_words_str);
    std::string word;
    while (std::getline(ss, word, ',')) {
        // Trim
        word.erase(0, word.find_first_not_of(" \t"));
        word.erase(word.find_last_not_of(" \t") + 1);
        if (!word.empty()) {
            wake_words_.push_back(word);
        }
    }
}

void TranscriptBuffer::append(const std::string& text, Speaker speaker) {
    if (text.empty()) return;

    std::lock_guard<std::mutex> lock(mutex_);

    TranscriptEntry entry{
        text,
        speaker,
        std::chrono::steady_clock::now(),
        false
    };

    entries_.push_back(entry);
    recent_entries_.push_back(entry);

    // Prevent unbounded growth
    if (entries_.size() > static_cast<size_t>(max_entries_)) {
        entries_.pop_front();
    }

    // Check for wake words
    std::string text_lower = text;
    std::transform(text_lower.begin(), text_lower.end(),
                   text_lower.begin(), ::tolower);
    for (const auto& ww : wake_words_) {
        if (text_lower.find(ww) != std::string::npos) {
            trigger_pending_ = true;
            break;
        }
    }
}

void TranscriptBuffer::bookmark(int seconds) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto cutoff = std::chrono::steady_clock::now() -
                  std::chrono::seconds(seconds);

    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        if (it->timestamp < cutoff) break;
        it->bookmarked = true;
    }
}

void TranscriptBuffer::set_trigger() {
    std::lock_guard<std::mutex> lock(mutex_);
    trigger_pending_ = true;
}

bool TranscriptBuffer::has_trigger() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return trigger_pending_;
}

std::string TranscriptBuffer::get_recent_and_clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    trigger_pending_ = false;

    if (recent_entries_.empty()) return "";

    std::ostringstream ss;
    for (const auto& entry : recent_entries_) {
        ss << entry.text << "\n";
    }
    recent_entries_.clear();

    return ss.str();
}

std::string TranscriptBuffer::get_full() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream ss;
    for (const auto& entry : entries_) {
        ss << entry.text << "\n";
    }
    return ss.str();
}

std::vector<TranscriptEntry> TranscriptBuffer::get_bookmarked() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<TranscriptEntry> result;
    for (const auto& entry : entries_) {
        if (entry.bookmarked) {
            result.push_back(entry);
        }
    }
    return result;
}

void TranscriptBuffer::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
    recent_entries_.clear();
    trigger_pending_ = false;
}

size_t TranscriptBuffer::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

}  // namespace orbit::asr
