/**
 * @file json_validator.h
 * @brief Post-generation JSON validation and repair.
 */
#pragma once

#include <string>

namespace orbit::slm {

class JsonValidator {
public:
    /** Check if a string is valid JSON. */
    static bool is_valid(const std::string& json);

    /** Attempt to repair common JSON issues (truncation, missing brackets). */
    static std::string repair(const std::string& json);

    /** Extract the first JSON object or array from a string. */
    static std::string extract_json(const std::string& text);
};

}  // namespace orbit::slm
