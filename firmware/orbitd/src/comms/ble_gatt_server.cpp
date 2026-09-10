/**
 * @file ble_gatt_server.cpp
 * @brief BLE GATT server implementation using BlueZ D-Bus API.
 */

#include "comms/ble_gatt_server.h"
#include "comms/json_serializer.h"
#include <iostream>
#include <cstring>

namespace orbit::comms {

BLEGATTServer::BLEGATTServer(const core::Config& config) {
    device_name_ = config.get_string("ble.device_name", "EarBrain");
    mtu_ = config.get_int("ble.mtu", 247);
}

BLEGATTServer::~BLEGATTServer() {
    shutdown();
}

bool BLEGATTServer::initialize() {
    // In production:
    // 1. Initialize D-Bus connection
    // 2. Register GATT application with BlueZ
    // 3. Define service UUID 0xEB01
    // 4. Register all characteristics (0xEB10-0xEB31)
    // 5. Start advertising

    std::cout << "[ble] GATT server initialized: " << device_name_
              << " (MTU=" << mtu_ << ")" << std::endl;

    // TODO: Full BlueZ D-Bus implementation
    // This is the interface contract — BlueZ integration is platform-specific

    return true;
}

void BLEGATTServer::process_events() {
    // In production: poll D-Bus for incoming write events
    // and connection state changes
}

void BLEGATTServer::shutdown() {
    connected_.store(false);
    std::cout << "[ble] GATT server shutdown" << std::endl;
}

void BLEGATTServer::notify_transcript(const std::string& text) {
    if (!connected_.load()) return;
    fragment_and_send(text, 0xEB10);
}

void BLEGATTServer::notify_action_items(const std::string& json) {
    if (!connected_.load()) return;
    fragment_and_send(json, 0xEB11);
}

void BLEGATTServer::notify_summary(const std::string& json) {
    if (!connected_.load()) return;
    fragment_and_send(json, 0xEB12);
}

void BLEGATTServer::notify_calendar(const std::string& json) {
    if (!connected_.load()) return;
    fragment_and_send(json, 0xEB15);
}

void BLEGATTServer::notify_crm(const std::string& json) {
    if (!connected_.load()) return;
    fragment_and_send(json, 0xEB16);
}

void BLEGATTServer::notify_email(const std::string& json) {
    if (!connected_.load()) return;
    fragment_and_send(json, 0xEB17);
}

void BLEGATTServer::notify_talk_time(float tx_ratio, float rx_ratio) {
    if (!connected_.load()) return;
    std::string json = "{\"tx_ratio\":" + std::to_string(tx_ratio) +
                       ",\"rx_ratio\":" + std::to_string(rx_ratio) + "}";
    fragment_and_send(json, 0xEB13);
}

void BLEGATTServer::notify_sentiment(const std::string& json) {
    if (!connected_.load()) return;
    fragment_and_send(json, 0xEB14);
}

void BLEGATTServer::notify_device_status(float battery_pct, float temp_c) {
    if (!connected_.load()) return;
    std::string json = "{\"battery_pct\":" + std::to_string(battery_pct) +
                       ",\"temp_c\":" + std::to_string(temp_c) + "}";
    fragment_and_send(json, 0xEB20);
}

std::optional<Command> BLEGATTServer::dequeue_command() {
    std::lock_guard<std::mutex> lock(cmd_mutex_);
    if (command_queue_.empty()) return std::nullopt;
    auto cmd = command_queue_.front();
    command_queue_.pop();
    return cmd;
}

void BLEGATTServer::fragment_and_send(const std::string& data,
                                        uint16_t characteristic_handle) {
    // Fragment data into MTU-sized chunks (minus 3 bytes for ATT header)
    const size_t payload_size = static_cast<size_t>(mtu_) - 3;
    size_t offset = 0;

    while (offset < data.size()) {
        size_t chunk_size = std::min(payload_size, data.size() - offset);

        // In production: send via BlueZ D-Bus GATT notification
        // dbus_send_notification(characteristic_handle,
        //                        data.data() + offset, chunk_size);

        bytes_sent_.fetch_add(chunk_size, std::memory_order_relaxed);
        offset += chunk_size;
    }
}

}  // namespace orbit::comms
