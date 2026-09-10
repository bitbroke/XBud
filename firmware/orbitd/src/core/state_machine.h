/**
 * @file state_machine.h
 * @brief Call lifecycle state machine for the orbitd daemon.
 *
 * States:
 *   IDLE → CONNECTING → CONSENT → ACTIVE_CALL → POST_CALL → SUMMARIZING → CLEANUP → IDLE
 *   ACTIVE_CALL ↔ THERMAL_THROTTLE (bidirectional on temperature events)
 */
#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include <chrono>

namespace orbit::core {

enum class CallState : uint8_t {
    IDLE = 0,           // No active call, low-power mode
    CONNECTING,         // HFP SCO link detected, setting up audio route
    CONSENT,            // Waiting for consent chime to play
    ACTIVE_CALL,        // Full pipeline active (VAD → ASR → NER → SLM)
    THERMAL_THROTTLE,   // SLM disabled, ASR-only mode
    POST_CALL,          // Call ended, preparing for final summary
    SUMMARIZING,        // Running final SLM summary pass
    CLEANUP,            // Wiping volatile memory, saving encrypted results
};

/**
 * @brief Returns a human-readable string for the given call state.
 */
const char* call_state_to_string(CallState state);

/**
 * @brief Callback type for state transition listeners.
 */
using StateChangeCallback = std::function<void(CallState old_state, CallState new_state)>;

/**
 * @brief Thread-safe state machine managing call lifecycle transitions.
 *
 * All transitions are validated against the allowed transition graph.
 * Invalid transitions are logged and rejected.
 */
class StateMachine {
public:
    StateMachine();
    ~StateMachine() = default;

    // Non-copyable, non-movable
    StateMachine(const StateMachine&) = delete;
    StateMachine& operator=(const StateMachine&) = delete;

    /**
     * @brief Get the current state (lock-free atomic read).
     */
    CallState current() const;

    /**
     * @brief Attempt a state transition. Returns true if the transition
     *        is valid and was applied.
     */
    bool transition(CallState new_state);

    /**
     * @brief Register a callback to be invoked on state transitions.
     *        Callbacks are invoked under the state mutex — keep them fast.
     */
    void on_state_change(StateChangeCallback callback);

    /**
     * @brief Get the timestamp of the last state transition.
     */
    std::chrono::steady_clock::time_point last_transition_time() const;

    /**
     * @brief Get the duration spent in the current state.
     */
    std::chrono::milliseconds time_in_current_state() const;

    /**
     * @brief Get call duration (time between ACTIVE_CALL entry and POST_CALL).
     */
    std::chrono::seconds call_duration() const;

private:
    /**
     * @brief Check if a transition from `from` to `to` is valid.
     */
    bool is_valid_transition(CallState from, CallState to) const;

    std::atomic<CallState> state_;
    mutable std::mutex mutex_;
    std::vector<StateChangeCallback> callbacks_;

    std::chrono::steady_clock::time_point last_transition_time_;
    std::chrono::steady_clock::time_point call_start_time_;
    std::chrono::steady_clock::time_point call_end_time_;
};

}  // namespace orbit::core
