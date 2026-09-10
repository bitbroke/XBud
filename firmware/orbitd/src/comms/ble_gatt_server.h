/**
 * @file ble_gatt_server.h
 * @brief BLE GATT server for pushing intelligence data to companion app.
 */
#pragma once

#include "core/config.h"
#include <string>
#include <queue>
#include <mutex>
#include <optional>
#include <atomic>

namespace orbit::comms {

enum class CommandType : uint8_t {
    TOGGLE_CONSENT = 0,
    BOOKMARK,
    UPLOAD_AGENDA,
    UNKNOWN,
};

struct Command {
    CommandType type;
    std::string payload;
};

class BLEGATTServer {
public:
    explicit BLEGATTServer(const core::Config& config);
    ~BLEGATTServer();

    bool initialize();
    void process_events();
    void shutdown();

    // Notification methods (push to companion app)
    void notify_transcript(const std::string& text);
    void notify_action_items(const std::string& json);
    void notify_summary(const std::string& json);
    void notify_calendar(const std::string& json);
    void notify_crm(const std::string& json);
    void notify_email(const std::string& json);
    void notify_talk_time(float tx_ratio, float rx_ratio);
    void notify_sentiment(const std::string& json);
    void notify_device_status(float battery_pct, float temp_c);

    // Command handling (receive from companion app)
    std::optional<Command> dequeue_command();

    bool is_connected() const { return connected_.load(); }
    uint64_t bytes_sent() const { return bytes_sent_.load(); }

private:
    void fragment_and_send(const std::string& data, uint16_t characteristic_handle);

    std::string device_name_;
    int mtu_ = 247;
    std::atomic<bool> connected_{false};
    std::atomic<uint64_t> bytes_sent_{0};

    std::mutex cmd_mutex_;
    std::queue<Command> command_queue_;

    // D-Bus / BlueZ handles would go here
    // Simplified for implementation reference
};

}  // namespace orbit::comms
