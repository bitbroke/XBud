/**
 * @file volatile_manager.cpp
 */
#include "privacy/volatile_manager.h"
#include <iostream>
#include <cstdlib>
#include <sys/mount.h>
#include <sys/stat.h>

namespace orbit::privacy {

VolatileManager::VolatileManager(const core::Config& config) {
    mount_path_ = config.get_string("privacy.volatile_path", "/volatile");
    max_size_mb_ = config.get_int("privacy.volatile_size_mb", 128);
}

VolatileManager::~VolatileManager() {
    secure_wipe();
}

bool VolatileManager::initialize() {
    // Ensure mount point exists
    mkdir(mount_path_.c_str(), 0700);

    // Mount tmpfs with strict permissions
    std::string opts = "size=" + std::to_string(max_size_mb_) +
                       "m,mode=0700,noexec,nosuid,nodev";

    int ret = mount("tmpfs", mount_path_.c_str(), "tmpfs", 0, opts.c_str());
    if (ret != 0) {
        std::cerr << "[volatile] mount failed (may already be mounted)" << std::endl;
        // Not fatal — may already be mounted via fstab
    }

    initialized_ = true;
    std::cout << "[volatile] Initialized: " << mount_path_
              << " (" << max_size_mb_ << " MB)" << std::endl;
    return true;
}

void VolatileManager::secure_wipe() {
    if (!initialized_) return;

    // Remove all files in the volatile directory
    std::string cmd = "find " + mount_path_ + " -type f -exec shred -n 1 -z -u {} \\;";
    system(cmd.c_str());

    std::cout << "[volatile] Secure wipe complete" << std::endl;
}

}  // namespace orbit::privacy
