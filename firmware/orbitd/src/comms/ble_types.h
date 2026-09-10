/**
 * @file ble_types.h
 * @brief BLE GATT service and characteristic UUID definitions.
 */
#pragma once

#include <cstdint>

namespace orbit::comms {

// Custom 128-bit base UUID: 0000XXXX-0000-1000-8000-00805f9b34fb
// Where XXXX is replaced by the 16-bit short UUID

namespace uuid {
    // Service
    constexpr uint16_t SERVICE_INTELLIGENCE = 0xEB01;

    // Characteristics
    constexpr uint16_t CHAR_LIVE_TRANSCRIPT = 0xEB10;
    constexpr uint16_t CHAR_ACTION_ITEMS    = 0xEB11;
    constexpr uint16_t CHAR_CALL_SUMMARY    = 0xEB12;
    constexpr uint16_t CHAR_TALK_TIME       = 0xEB13;
    constexpr uint16_t CHAR_SENTIMENT       = 0xEB14;
    constexpr uint16_t CHAR_CALENDAR_EVENT  = 0xEB15;
    constexpr uint16_t CHAR_CRM_PAYLOAD     = 0xEB16;
    constexpr uint16_t CHAR_EMAIL_DRAFT     = 0xEB17;
    constexpr uint16_t CHAR_DEVICE_STATUS   = 0xEB20;
    constexpr uint16_t CHAR_CONSENT_STATE   = 0xEB21;
    constexpr uint16_t CHAR_AGENDA_UPLOAD   = 0xEB22;
    constexpr uint16_t CHAR_COMMAND         = 0xEB30;
    constexpr uint16_t CHAR_BOOKMARK        = 0xEB31;
}

}  // namespace orbit::comms
