/**
 * @file config.h
 * @brief Runtime configuration management for orbitd.
 *
 * Reads a simple INI-style configuration file and provides typed accessors.
 * Supports hot-reload via SIGHUP.
 */
#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

namespace orbit::core {

/**
 * @brief Configuration manager for the orbitd daemon.
 *
 * File format (INI-like):
 *   # Comment
 *   [section]
 *   key = value
 *
 * Access via dot notation: config.get_string("section.key", default)
 */
class Config {
public:
    Config() = default;
    ~Config() = default;

    /**
     * @brief Load (or reload) configuration from a file.
     * @param path Absolute path to the config file.
     * @return true if the file was parsed successfully.
     */
    bool load(const std::string& path);

    /**
     * @brief Get a string value.
     * @param key Dot-separated key (e.g., "audio.sample_rate").
     * @param default_val Value returned if key is not found.
     */
    std::string get_string(const std::string& key,
                           const std::string& default_val = "") const;

    /**
     * @brief Get an integer value.
     */
    int get_int(const std::string& key, int default_val = 0) const;

    /**
     * @brief Get a floating-point value.
     */
    float get_float(const std::string& key, float default_val = 0.0f) const;

    /**
     * @brief Get a boolean value ("true", "1", "yes" → true).
     */
    bool get_bool(const std::string& key, bool default_val = false) const;

    /**
     * @brief Check if a key exists.
     */
    bool has(const std::string& key) const;

    /**
     * @brief Get the path the config was loaded from.
     */
    const std::string& file_path() const { return file_path_; }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> values_;
    std::string file_path_;
};

}  // namespace orbit::core
