/**
 * @file regex_prefilter.h
 * @brief Fast regex-based PII pre-filter for known Indian document patterns.
 *
 * Catches Aadhaar (12-digit), PAN (XXXPX1234X), phone numbers, credit cards,
 * and UPI IDs with near-zero latency (~0.1ms) before the heavier NER model.
 */
#pragma once

#include <string>
#include <vector>
#include <regex>

namespace orbit::ner {

struct PIIMatch {
    std::string entity_type;    // "AADHAAR", "PAN", "PHONE", "CARD", "UPI"
    std::string replacement;    // "[GOV_ID]", "[GOV_ID]", "[PHONE]", "[CARD]", "[UPI]"
    size_t start;
    size_t end;
};

class RegexPrefilter {
public:
    RegexPrefilter();
    ~RegexPrefilter() = default;

    /**
     * @brief Apply regex patterns to detect and replace known PII formats.
     * @param text Input text.
     * @return Text with pattern-matched PII replaced by tokens.
     */
    std::string filter(const std::string& text) const;

    /**
     * @brief Get all PII matches without replacing.
     */
    std::vector<PIIMatch> detect(const std::string& text) const;

private:
    struct Pattern {
        std::regex regex;
        std::string entity_type;
        std::string replacement;
    };

    std::vector<Pattern> patterns_;
};

}  // namespace orbit::ner
