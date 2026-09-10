/**
 * @file state_machine.cpp
 * @brief Implementation of the call lifecycle state machine.
 */

#include "core/state_machine.h"
#include <iostream>
#include <unordered_map>
#include <unordered_set>

namespace orbit::core {

// ---------------------------------------------------------------------------
// Allowed state transitions (adjacency list)
// ---------------------------------------------------------------------------
static const std::unordered_map<CallState, std::unordered_set<CallState>> VALID_TRANSITIONS = {
    {CallState::IDLE,             {CallState::CONNECTING}},
    {CallState::CONNECTING,       {CallState::CONSENT, CallState::IDLE}},
    {CallState::CONSENT,          {CallState::ACTIVE_CALL, CallState::IDLE}},
    {CallState::ACTIVE_CALL,      {CallState::POST_CALL, CallState::THERMAL_THROTTLE}},
    {CallState::THERMAL_THROTTLE, {CallState::ACTIVE_CALL, CallState::POST_CALL}},
    {CallState::POST_CALL,        {CallState::SUMMARIZING}},
    {CallState::SUMMARIZING,      {CallState::CLEANUP}},
    {CallState::CLEANUP,          {CallState::IDLE}},
};

const char* call_state_to_string(CallState state) {
    switch (state) {
        case CallState::IDLE:             return "IDLE";
        case CallState::CONNECTING:       return "CONNECTING";
        case CallState::CONSENT:          return "CONSENT";
        case CallState::ACTIVE_CALL:      return "ACTIVE_CALL";
        case CallState::THERMAL_THROTTLE: return "THERMAL_THROTTLE";
        case CallState::POST_CALL:        return "POST_CALL";
        case CallState::SUMMARIZING:      return "SUMMARIZING";
        case CallState::CLEANUP:          return "CLEANUP";
        default:                          return "UNKNOWN";
    }
}

StateMachine::StateMachine()
    : state_(CallState::IDLE)
    , last_transition_time_(std::chrono::steady_clock::now())
    , call_start_time_{}
    , call_end_time_{}
{}

CallState StateMachine::current() const {
    return state_.load(std::memory_order_acquire);
}

bool StateMachine::transition(CallState new_state) {
    std::lock_guard<std::mutex> lock(mutex_);

    CallState old_state = state_.load(std::memory_order_relaxed);

    if (!is_valid_transition(old_state, new_state)) {
        std::cerr << "[state] Invalid transition: "
                  << call_state_to_string(old_state) << " → "
                  << call_state_to_string(new_state) << std::endl;
        return false;
    }

    // Track call timing
    auto now = std::chrono::steady_clock::now();

    if (new_state == CallState::ACTIVE_CALL &&
        old_state != CallState::THERMAL_THROTTLE) {
        call_start_time_ = now;
    }

    if (new_state == CallState::POST_CALL) {
        call_end_time_ = now;
    }

    // Apply transition
    state_.store(new_state, std::memory_order_release);
    last_transition_time_ = now;

    std::cout << "[state] " << call_state_to_string(old_state)
              << " → " << call_state_to_string(new_state) << std::endl;

    // Notify listeners
    for (const auto& cb : callbacks_) {
        cb(old_state, new_state);
    }

    return true;
}

void StateMachine::on_state_change(StateChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.push_back(std::move(callback));
}

std::chrono::steady_clock::time_point StateMachine::last_transition_time() const {
    return last_transition_time_;
}

std::chrono::milliseconds StateMachine::time_in_current_state() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_transition_time_
    );
}

std::chrono::seconds StateMachine::call_duration() const {
    if (call_start_time_ == std::chrono::steady_clock::time_point{}) {
        return std::chrono::seconds(0);
    }

    auto end = (call_end_time_ != std::chrono::steady_clock::time_point{})
        ? call_end_time_
        : std::chrono::steady_clock::now();

    return std::chrono::duration_cast<std::chrono::seconds>(end - call_start_time_);
}

bool StateMachine::is_valid_transition(CallState from, CallState to) const {
    auto it = VALID_TRANSITIONS.find(from);
    if (it == VALID_TRANSITIONS.end()) return false;
    return it->second.count(to) > 0;
}

}  // namespace orbit::core
