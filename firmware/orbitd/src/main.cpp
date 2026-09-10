/**
 * @file main.cpp
 * @brief Ear-Brain orbitd daemon entry point.
 *
 * This is the single C++20 daemon process that runs on the RK3576 SoC inside
 * the smart charging case. It orchestrates the entire AI pipeline:
 *   Audio Capture → VAD → ASR → NER/PII → SLM → BLE GATT
 *
 * Design principles:
 *   - Single process, multi-threaded (SCHED_FIFO for real-time threads)
 *   - Zero network access (enforced by seccomp)
 *   - All audio in volatile RAM (tmpfs), never persisted as raw PCM
 *   - Graceful degradation under thermal pressure
 */

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <atomic>
#include <memory>

#include "core/config.h"
#include "core/state_machine.h"
#include "core/watchdog.h"
#include "core/thermal_governor.h"
#include "audio/i2s_capture.h"
#include "audio/ring_buffer.h"
#include "audio/consent_chime.h"
#include "vad/silero_vad.h"
#include "vad/energy_detector.h"
#include "asr/whisper_engine.h"
#include "asr/transcript_buffer.h"
#include "ner/pii_scrubber.h"
#include "slm/gemma_engine.h"
#include "slm/prompt_templates.h"
#include "comms/ble_gatt_server.h"
#include "comms/json_serializer.h"
#include "storage/encrypted_db.h"
#include "storage/call_repository.h"
#include "privacy/volatile_manager.h"
#include "privacy/consent_manager.h"
#include "privacy/sandbox.h"

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------
static std::atomic<bool> g_running{true};
static std::atomic<bool> g_reload_config{false};

static void signal_handler(int sig) {
    switch (sig) {
        case SIGTERM:
        case SIGINT:
            g_running.store(false, std::memory_order_release);
            break;
        case SIGHUP:
            g_reload_config.store(true, std::memory_order_release);
            break;
        default:
            break;
    }
}

static void install_signal_handlers() {
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGHUP, &sa, nullptr);

    // Ignore SIGPIPE — BLE writes may fail if client disconnects
    signal(SIGPIPE, SIG_IGN);
}

// ---------------------------------------------------------------------------
// Thread entry points
// ---------------------------------------------------------------------------

/**
 * Audio capture thread — SCHED_FIFO priority 99, pinned to CPU 4 (A72).
 * Reads I2S PCM frames in 10ms cycles and writes to the lock-free ring buffer.
 */
static void audio_capture_thread(
    orbit::audio::I2SCapture& capture,
    orbit::audio::RingBuffer& ring_buffer,
    const std::atomic<bool>& running
) {
    // Set thread name for debugging
    pthread_setname_np(pthread_self(), "orbit-audio");

    while (running.load(std::memory_order_acquire)) {
        auto frames = capture.read_frames();
        if (!frames.empty()) {
            ring_buffer.write(frames);
        }
    }
}

/**
 * VAD + ASR thread — SCHED_FIFO priority 90, pinned to CPU 5-6 (A72).
 * Processes 30ms audio chunks through Silero VAD, then feeds speech segments
 * to IndicWhisper for transcription.
 */
static void vad_asr_thread(
    orbit::audio::RingBuffer& ring_buffer,
    orbit::vad::SileroVAD& vad,
    orbit::vad::EnergyDetector& energy,
    orbit::asr::WhisperEngine& whisper,
    orbit::asr::TranscriptBuffer& transcript_buf,
    orbit::ner::PIIScrubber& scrubber,
    orbit::comms::BLEGATTServer& ble,
    orbit::core::StateMachine& state,
    const std::atomic<bool>& running
) {
    pthread_setname_np(pthread_self(), "orbit-vad-asr");

    std::vector<int16_t> chunk;
    constexpr size_t CHUNK_SAMPLES = 480;  // 30ms at 16kHz

    while (running.load(std::memory_order_acquire)) {
        if (state.current() != orbit::core::CallState::ACTIVE_CALL &&
            state.current() != orbit::core::CallState::THERMAL_THROTTLE) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        chunk.resize(CHUNK_SAMPLES * 2);  // 2 channels
        if (!ring_buffer.read(chunk)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        // Split channels
        std::vector<int16_t> tx_chunk(CHUNK_SAMPLES);
        std::vector<int16_t> rx_chunk(CHUNK_SAMPLES);
        for (size_t i = 0; i < CHUNK_SAMPLES; ++i) {
            tx_chunk[i] = chunk[i * 2];      // User mic
            rx_chunk[i] = chunk[i * 2 + 1];  // Caller audio
        }

        // Energy detection for talk-time analytics (zero-cost)
        energy.process(tx_chunk, rx_chunk);

        // VAD on both channels
        float tx_prob = vad.process(tx_chunk);
        float rx_prob = vad.process(rx_chunk);

        bool tx_speech = tx_prob > 0.5f;
        bool rx_speech = rx_prob > 0.5f;

        // Feed speech segments to Whisper ASR
        if (tx_speech) {
            auto text = whisper.transcribe_segment(tx_chunk, orbit::asr::Speaker::USER);
            if (!text.empty()) {
                auto sanitized = scrubber.scrub(text);
                transcript_buf.append(sanitized, orbit::asr::Speaker::USER);

                // Push live transcript via BLE
                ble.notify_transcript(sanitized);
            }
        }
        if (rx_speech) {
            auto text = whisper.transcribe_segment(rx_chunk, orbit::asr::Speaker::CALLER);
            if (!text.empty()) {
                auto sanitized = scrubber.scrub(text);
                transcript_buf.append(sanitized, orbit::asr::Speaker::CALLER);
                ble.notify_transcript(sanitized);
            }
        }
    }
}

/**
 * SLM inference thread — normal priority, uses all A72 cores (CPU 4-7).
 * Runs in batched mode: processes accumulated transcript every 30s or on
 * wake-word trigger. Disabled during thermal throttle.
 */
static void slm_inference_thread(
    orbit::asr::TranscriptBuffer& transcript_buf,
    orbit::slm::GemmaEngine& gemma,
    orbit::slm::PromptTemplates& prompts,
    orbit::comms::BLEGATTServer& ble,
    orbit::storage::CallRepository& repo,
    orbit::core::StateMachine& state,
    orbit::core::ThermalGovernor& thermal,
    const std::atomic<bool>& running
) {
    pthread_setname_np(pthread_self(), "orbit-slm");

    auto last_inference = std::chrono::steady_clock::now();
    auto interval = std::chrono::seconds(30);

    while (running.load(std::memory_order_acquire)) {
        // Sleep if not in active call or if thermally throttled
        if (state.current() == orbit::core::CallState::THERMAL_THROTTLE) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        if (state.current() != orbit::core::CallState::ACTIVE_CALL) {
            // Check for post-call summarization
            if (state.current() == orbit::core::CallState::SUMMARIZING) {
                auto full_transcript = transcript_buf.get_full();
                if (!full_transcript.empty()) {
                    auto summary_prompt = prompts.build_summary(full_transcript);
                    auto summary = gemma.generate(summary_prompt, "call_summary");
                    ble.notify_summary(summary);
                    repo.save_summary(summary);

                    auto crm_prompt = prompts.build_crm_payload(full_transcript);
                    auto crm = gemma.generate(crm_prompt, "crm_payload");
                    ble.notify_crm(crm);

                    auto email_prompt = prompts.build_email_draft(full_transcript);
                    auto email = gemma.generate(email_prompt, "email_draft");
                    ble.notify_email(email);

                    state.transition(orbit::core::CallState::CLEANUP);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        // Batched inference every N seconds
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_inference);

        // Adjust interval based on thermal state
        interval = std::chrono::seconds(thermal.get_slm_interval_s());

        if (elapsed >= interval || transcript_buf.has_trigger()) {
            auto recent = transcript_buf.get_recent_and_clear();
            if (!recent.empty()) {
                // Extract action items
                auto action_prompt = prompts.build_action_items(recent);
                auto actions = gemma.generate(action_prompt, "action_item");
                if (!actions.empty()) {
                    ble.notify_action_items(actions);
                }

                // Extract deadlines / calendar events
                auto calendar_prompt = prompts.build_calendar_events(recent);
                auto events = gemma.generate(calendar_prompt, "calendar_event");
                if (!events.empty()) {
                    ble.notify_calendar(events);
                }

                // Decision logging
                auto decision_prompt = prompts.build_decisions(recent);
                auto decisions = gemma.generate(decision_prompt, "decision_entry");
                if (!decisions.empty()) {
                    ble.notify_action_items(decisions);  // Re-use action items channel
                }

                last_inference = now;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

/**
 * Thermal monitor thread — SCHED_FIFO priority 95, pinned to CPU 1 (A53).
 * Polls thermal sensors every 1s and adjusts system state accordingly.
 */
static void thermal_monitor_thread(
    orbit::core::ThermalGovernor& thermal,
    orbit::core::StateMachine& state,
    const std::atomic<bool>& running
) {
    pthread_setname_np(pthread_self(), "orbit-thermal");

    while (running.load(std::memory_order_acquire)) {
        thermal.poll();

        if (thermal.is_emergency()) {
            // Emergency shutdown — save state and halt
            state.transition(orbit::core::CallState::CLEANUP);
            g_running.store(false, std::memory_order_release);
            break;
        }

        if (thermal.is_critical() &&
            state.current() == orbit::core::CallState::ACTIVE_CALL) {
            state.transition(orbit::core::CallState::THERMAL_THROTTLE);
        }

        if (!thermal.is_critical() &&
            state.current() == orbit::core::CallState::THERMAL_THROTTLE) {
            state.transition(orbit::core::CallState::ACTIVE_CALL);
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

/**
 * BLE communications thread — normal priority, pinned to CPU 2 (A53).
 * Handles GATT server events and incoming commands from the companion app.
 */
static void ble_comms_thread(
    orbit::comms::BLEGATTServer& ble,
    orbit::core::StateMachine& state,
    orbit::privacy::ConsentManager& consent,
    const std::atomic<bool>& running
) {
    pthread_setname_np(pthread_self(), "orbit-ble");

    while (running.load(std::memory_order_acquire)) {
        ble.process_events();

        // Handle incoming commands
        auto cmd = ble.dequeue_command();
        if (cmd.has_value()) {
            switch (cmd->type) {
                case orbit::comms::CommandType::TOGGLE_CONSENT:
                    consent.toggle();
                    break;
                case orbit::comms::CommandType::BOOKMARK:
                    // Flag last 30s of transcript as high-priority
                    break;
                case orbit::comms::CommandType::UPLOAD_AGENDA:
                    // Store pre-call agenda for future use (v2)
                    break;
                default:
                    break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

/**
 * Watchdog thread — SCHED_FIFO priority 99, pinned to CPU 0 (A53).
 * Feeds the hardware watchdog and monitors thread health.
 */
static void watchdog_thread(
    orbit::core::Watchdog& watchdog,
    const std::atomic<bool>& running
) {
    pthread_setname_np(pthread_self(), "orbit-wdog");

    while (running.load(std::memory_order_acquire)) {
        watchdog.feed();
        watchdog.check_thread_health();
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    // Parse config file path from args or use default
    std::string config_path = "/opt/orbitd/etc/orbitd.conf";
    if (argc > 1) {
        config_path = argv[1];
    }

    std::cout << "[orbitd] Ear-Brain Daemon v" << ORBITD_VERSION_STRING << std::endl;
    std::cout << "[orbitd] Loading config from: " << config_path << std::endl;

    // -----------------------------------------------------------------------
    // 1. Load configuration
    // -----------------------------------------------------------------------
    orbit::core::Config config;
    if (!config.load(config_path)) {
        std::cerr << "[orbitd] FATAL: Failed to load config" << std::endl;
        return EXIT_FAILURE;
    }

    // -----------------------------------------------------------------------
    // 2. Drop privileges and apply seccomp sandbox
    // -----------------------------------------------------------------------
    orbit::privacy::Sandbox sandbox;
    if (config.get_bool("security.sandbox_enabled", true)) {
        if (!sandbox.apply()) {
            std::cerr << "[orbitd] FATAL: Sandbox setup failed" << std::endl;
            return EXIT_FAILURE;
        }
        std::cout << "[orbitd] Security sandbox applied" << std::endl;
    }

    // -----------------------------------------------------------------------
    // 3. Initialize volatile memory (tmpfs)
    // -----------------------------------------------------------------------
    orbit::privacy::VolatileManager volatile_mgr(config);
    if (!volatile_mgr.initialize()) {
        std::cerr << "[orbitd] FATAL: Volatile memory init failed" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "[orbitd] Volatile memory initialized (/volatile)" << std::endl;

    // -----------------------------------------------------------------------
    // 4. Initialize subsystems
    // -----------------------------------------------------------------------
    orbit::core::StateMachine state;
    orbit::core::ThermalGovernor thermal(config);
    orbit::core::Watchdog watchdog(config);
    orbit::privacy::ConsentManager consent(config);

    // Audio subsystem
    orbit::audio::RingBuffer ring_buffer(
        config.get_int("audio.ring_buffer_seconds", 60),
        config.get_int("audio.sample_rate", 16000),
        config.get_int("audio.channels", 2)
    );
    orbit::audio::I2SCapture capture(config, ring_buffer);
    orbit::audio::ConsentChime chime(config);

    // AI pipeline
    orbit::vad::SileroVAD vad(config);
    orbit::vad::EnergyDetector energy;
    orbit::asr::WhisperEngine whisper(config);
    orbit::asr::TranscriptBuffer transcript_buf(config);
    orbit::ner::PIIScrubber scrubber(config);
    orbit::slm::GemmaEngine gemma(config);
    orbit::slm::PromptTemplates prompts(config);

    // Communications
    orbit::comms::BLEGATTServer ble(config);

    // Storage
    orbit::storage::EncryptedDB db(config);
    orbit::storage::CallRepository repo(db);

    // -----------------------------------------------------------------------
    // 5. Install signal handlers
    // -----------------------------------------------------------------------
    install_signal_handlers();

    // -----------------------------------------------------------------------
    // 6. Initialize all components
    // -----------------------------------------------------------------------
    std::cout << "[orbitd] Initializing AI models..." << std::endl;

    if (!vad.initialize()) {
        std::cerr << "[orbitd] FATAL: VAD init failed" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "[orbitd] ✓ Silero VAD loaded (~2MB)" << std::endl;

    if (!whisper.initialize()) {
        std::cerr << "[orbitd] FATAL: Whisper ASR init failed" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "[orbitd] ✓ IndicWhisper Base loaded (~350MB)" << std::endl;

    if (!scrubber.initialize()) {
        std::cerr << "[orbitd] FATAL: NER scrubber init failed" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "[orbitd] ✓ DistilBERT NER loaded (~180MB)" << std::endl;

    if (!gemma.initialize()) {
        std::cerr << "[orbitd] FATAL: Gemma SLM init failed" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "[orbitd] ✓ Gemma 3 1B loaded (~1200MB)" << std::endl;

    if (!ble.initialize()) {
        std::cerr << "[orbitd] FATAL: BLE GATT init failed" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "[orbitd] ✓ BLE GATT server started" << std::endl;

    if (!db.open()) {
        std::cerr << "[orbitd] FATAL: Database init failed" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "[orbitd] ✓ SQLCipher database opened" << std::endl;

    if (!capture.initialize()) {
        std::cerr << "[orbitd] FATAL: I2S audio init failed" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "[orbitd] ✓ I2S audio capture ready" << std::endl;

    if (!watchdog.initialize()) {
        std::cerr << "[orbitd] WARNING: Hardware watchdog unavailable" << std::endl;
    } else {
        std::cout << "[orbitd] ✓ Hardware watchdog active" << std::endl;
    }

    // -----------------------------------------------------------------------
    // 7. Launch threads
    // -----------------------------------------------------------------------
    std::cout << "[orbitd] Starting threads..." << std::endl;

    state.transition(orbit::core::CallState::IDLE);

    std::thread t_audio(audio_capture_thread,
        std::ref(capture), std::ref(ring_buffer), std::cref(g_running));

    std::thread t_vad_asr(vad_asr_thread,
        std::ref(ring_buffer), std::ref(vad), std::ref(energy),
        std::ref(whisper), std::ref(transcript_buf), std::ref(scrubber),
        std::ref(ble), std::ref(state), std::cref(g_running));

    std::thread t_slm(slm_inference_thread,
        std::ref(transcript_buf), std::ref(gemma), std::ref(prompts),
        std::ref(ble), std::ref(repo), std::ref(state),
        std::ref(thermal), std::cref(g_running));

    std::thread t_thermal(thermal_monitor_thread,
        std::ref(thermal), std::ref(state), std::cref(g_running));

    std::thread t_ble(ble_comms_thread,
        std::ref(ble), std::ref(state), std::ref(consent),
        std::cref(g_running));

    std::thread t_watchdog(watchdog_thread,
        std::ref(watchdog), std::cref(g_running));

    std::cout << "[orbitd] All threads running. Waiting for calls..." << std::endl;

    // -----------------------------------------------------------------------
    // 8. Main loop — handle config reloads and state transitions
    // -----------------------------------------------------------------------
    while (g_running.load(std::memory_order_acquire)) {
        if (g_reload_config.exchange(false, std::memory_order_acq_rel)) {
            std::cout << "[orbitd] Reloading configuration..." << std::endl;
            config.load(config_path);
            thermal.reload(config);
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // -----------------------------------------------------------------------
    // 9. Graceful shutdown
    // -----------------------------------------------------------------------
    std::cout << "[orbitd] Shutting down..." << std::endl;

    // Join all threads
    if (t_audio.joinable()) t_audio.join();
    if (t_vad_asr.joinable()) t_vad_asr.join();
    if (t_slm.joinable()) t_slm.join();
    if (t_thermal.joinable()) t_thermal.join();
    if (t_ble.joinable()) t_ble.join();
    if (t_watchdog.joinable()) t_watchdog.join();

    // Cleanup
    volatile_mgr.secure_wipe();
    db.close();
    ble.shutdown();
    capture.shutdown();

    std::cout << "[orbitd] Clean shutdown complete." << std::endl;
    return EXIT_SUCCESS;
}
