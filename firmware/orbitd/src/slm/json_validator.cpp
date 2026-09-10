/**
 * @file json_validator.cpp
 * @brief JSON validation and repair for SLM output.
 */

#include "slm/json_validator.h"
#include <stack>
#include <algorithm>

namespace orbit::slm {

bool JsonValidator::is_valid(const std::string& json) {
    if (json.empty()) return false;

    // Simple bracket-matching validation
    std::stack<char> brackets;
    bool in_string = false;
    bool escaped = false;

    for (char c : json) {
        if (escaped) {
            escaped = false;
            continue;
        }
        if (c == '\\' && in_string) {
            escaped = true;
            continue;
        }
        if (c == '"') {
            in_string = !in_string;
            continue;
        }
        if (in_string) continue;

        if (c == '{' || c == '[') {
            brackets.push(c);
        } else if (c == '}') {
            if (brackets.empty() || brackets.top() != '{') return false;
            brackets.pop();
        } else if (c == ']') {
            if (brackets.empty() || brackets.top() != '[') return false;
            brackets.pop();
        }
    }

    return brackets.empty() && !in_string;
}

std::string JsonValidator::repair(const std::string& json) {
    if (json.empty()) return "{}";

    std::string result = json;

    // Trim trailing whitespace
    while (!result.empty() && (result.back() == ' ' || result.back() == '\n' ||
                                result.back() == '\r' || result.back() == '\t')) {
        result.pop_back();
    }

    // Count open/close brackets
    int open_braces = 0, close_braces = 0;
    int open_brackets = 0, close_brackets = 0;
    bool in_string = false;
    bool escaped = false;

    for (char c : result) {
        if (escaped) { escaped = false; continue; }
        if (c == '\\' && in_string) { escaped = true; continue; }
        if (c == '"') { in_string = !in_string; continue; }
        if (in_string) continue;

        if (c == '{') open_braces++;
        else if (c == '}') close_braces++;
        else if (c == '[') open_brackets++;
        else if (c == ']') close_brackets++;
    }

    // Close unclosed strings
    if (in_string) {
        result += '"';
    }

    // Remove trailing comma before closing bracket
    size_t last_non_ws = result.find_last_not_of(" \t\n\r");
    if (last_non_ws != std::string::npos && result[last_non_ws] == ',') {
        result.erase(last_non_ws, 1);
    }

    // Add missing closing brackets
    while (close_braces < open_braces) {
        result += '}';
        close_braces++;
    }
    while (close_brackets < open_brackets) {
        result += ']';
        close_brackets++;
    }

    return result;
}

std::string JsonValidator::extract_json(const std::string& text) {
    // Find first { or [
    size_t start = text.find_first_of("{[");
    if (start == std::string::npos) return "";

    char open = text[start];
    char close = (open == '{') ? '}' : ']';

    int depth = 0;
    bool in_string = false;
    bool escaped = false;

    for (size_t i = start; i < text.size(); ++i) {
        char c = text[i];

        if (escaped) { escaped = false; continue; }
        if (c == '\\' && in_string) { escaped = true; continue; }
        if (c == '"') { in_string = !in_string; continue; }
        if (in_string) continue;

        if (c == open) depth++;
        else if (c == close) {
            depth--;
            if (depth == 0) {
                return text.substr(start, i - start + 1);
            }
        }
    }

    // Truncated — return what we have and repair
    return repair(text.substr(start));
}

}  // namespace orbit::slm
