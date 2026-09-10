/**
 * @file volatile_manager.h / .cpp
 * @brief Volatile memory (tmpfs) lifecycle management.
 */
#pragma once
#include "core/config.h"
#include <string>

namespace orbit::privacy {

class VolatileManager {
public:
    explicit VolatileManager(const core::Config& config);
    ~VolatileManager();

    bool initialize();
    void secure_wipe();
    const std::string& mount_path() const { return mount_path_; }

private:
    std::string mount_path_;
    size_t max_size_mb_;
    bool initialized_ = false;
};

}  // namespace orbit::privacy
