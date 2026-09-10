/**
 * @file consent_manager.cpp
 */
#include "privacy/consent_manager.h"
#include <iostream>

namespace orbit::privacy {

ConsentManager::ConsentManager(const core::Config& config) {
    enabled_.store(config.get_bool("consent.default_enabled", true));
    require_per_call_ = config.get_bool("consent.require_per_call", false);
}

void ConsentManager::toggle() {
    bool current = enabled_.load();
    enabled_.store(!current);
    std::cout << "[consent] " << (enabled_.load() ? "ENABLED" : "DISABLED") << std::endl;
}

}  // namespace orbit::privacy
