/**
 * @file consent_manager.h / .cpp
 * @brief Consent state management for DPDP Act compliance.
 */
#pragma once
#include "core/config.h"
#include <atomic>

namespace orbit::privacy {

class ConsentManager {
public:
    explicit ConsentManager(const core::Config& config);
    bool is_enabled() const { return enabled_.load(); }
    void toggle();
    void enable() { enabled_.store(true); }
    void disable() { enabled_.store(false); }

private:
    std::atomic<bool> enabled_{true};
    bool require_per_call_;  // If true, consent resets each call
};

}  // namespace orbit::privacy
