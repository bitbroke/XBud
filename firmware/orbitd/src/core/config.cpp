/**
 * @file config.cpp
 * @brief INI-style configuration parser implementation.
 */

#include "core/config.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace orbit::core {

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    auto end = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool Config::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[config] Failed to open: " << path << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    values_.clear();
    file_path_ = path;

    std::string line;
    std::string current_section;

    while (std::getline(file, line)) {
        line = trim(line);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // Section header: [section_name]
        if (line[0] == '[' && line.back() == ']') {
            current_section = trim(line.substr(1, line.size() - 2));
            continue;
        }

        // Key = value
        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            std::cerr << "[config] Skipping malformed line: " << line << std::endl;
            continue;
        }

        std::string key = trim(line.substr(0, eq_pos));
        std::string value = trim(line.substr(eq_pos + 1));

        // Remove inline comments
        auto comment_pos = value.find('#');
        if (comment_pos != std::string::npos) {
            value = trim(value.substr(0, comment_pos));
        }

        // Store as "section.key"
        std::string full_key = current_section.empty()
            ? key
            : current_section + "." + key;

        values_[full_key] = value;
    }

    std::cout << "[config] Loaded " << values_.size()
              << " settings from " << path << std::endl;
    return true;
}

std::string Config::get_string(const std::string& key,
                                const std::string& default_val) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = values_.find(key);
    return (it != values_.end()) ? it->second : default_val;
}

int Config::get_int(const std::string& key, int default_val) const {
    auto val = get_string(key, "");
    if (val.empty()) return default_val;
    try {
        return std::stoi(val);
    } catch (...) {
        return default_val;
    }
}

float Config::get_float(const std::string& key, float default_val) const {
    auto val = get_string(key, "");
    if (val.empty()) return default_val;
    try {
        return std::stof(val);
    } catch (...) {
        return default_val;
    }
}

bool Config::get_bool(const std::string& key, bool default_val) const {
    auto val = to_lower(get_string(key, ""));
    if (val.empty()) return default_val;
    return (val == "true" || val == "1" || val == "yes" || val == "on");
}

bool Config::has(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return values_.count(key) > 0;
}

}  // namespace orbit::core
