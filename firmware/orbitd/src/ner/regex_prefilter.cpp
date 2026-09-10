/**
 * @file regex_prefilter.cpp
 * @brief Fast regex PII pre-filter implementation.
 */

#include "ner/regex_prefilter.h"

namespace orbit::ner {

RegexPrefilter::RegexPrefilter() {
    // Aadhaar: 12 digits, optionally grouped as 4-4-4 or with spaces
    patterns_.push_back({
        std::regex(R"(\b\d{4}[\s-]?\d{4}[\s-]?\d{4}\b)"),
        "AADHAAR",
        "[GOV_ID]"
    });

    // PAN: 5 letters + 4 digits + 1 letter (e.g., ABCPD1234E)
    patterns_.push_back({
        std::regex(R"(\b[A-Z]{5}\d{4}[A-Z]\b)", std::regex::icase),
        "PAN",
        "[GOV_ID]"
    });

    // Indian phone numbers: +91 followed by 10 digits, various separators
    patterns_.push_back({
        std::regex(R"((?:\+91[\s-]?|0)?[6-9]\d{9}\b)"),
        "PHONE",
        "[PHONE]"
    });

    // Credit/debit card numbers: 16 digits grouped as 4-4-4-4
    patterns_.push_back({
        std::regex(R"(\b\d{4}[\s-]?\d{4}[\s-]?\d{4}[\s-]?\d{4}\b)"),
        "CARD",
        "[CARD]"
    });

    // UPI IDs: something@bankcode
    patterns_.push_back({
        std::regex(R"(\b[a-zA-Z0-9._-]+@[a-zA-Z]{2,}\b)"),
        "UPI",
        "[UPI]"
    });

    // Email addresses
    patterns_.push_back({
        std::regex(R"(\b[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}\b)"),
        "EMAIL",
        "[EMAIL]"
    });

    // Indian PIN codes: 6 digits
    patterns_.push_back({
        std::regex(R"(\b[1-9]\d{5}\b)"),
        "PINCODE",
        "[PINCODE]"
    });
}

std::string RegexPrefilter::filter(const std::string& text) const {
    std::string result = text;

    // Apply patterns in priority order (most specific first)
    // Credit cards before Aadhaar (16 digits vs 12 digits)
    for (const auto& pattern : patterns_) {
        result = std::regex_replace(result, pattern.regex, pattern.replacement);
    }

    return result;
}

std::vector<PIIMatch> RegexPrefilter::detect(const std::string& text) const {
    std::vector<PIIMatch> matches;

    for (const auto& pattern : patterns_) {
        auto begin = std::sregex_iterator(text.begin(), text.end(), pattern.regex);
        auto end = std::sregex_iterator();

        for (auto it = begin; it != end; ++it) {
            PIIMatch match;
            match.entity_type = pattern.entity_type;
            match.replacement = pattern.replacement;
            match.start = static_cast<size_t>(it->position());
            match.end = match.start + static_cast<size_t>(it->length());
            matches.push_back(match);
        }
    }

    return matches;
}

}  // namespace orbit::ner
