/**
 * @file json_serializer.h
 * @brief JSON serialization utilities for BLE payload construction.
 */
#pragma once

#include <string>

namespace orbit::comms {

class JsonSerializer {
public:
    /** Escape special characters for JSON string values. */
    static std::string escape(const std::string& input);

    /** Compute the byte size of a UTF-8 string. */
    static size_t byte_size(const std::string& s) { return s.size(); }

    /** Check if a payload fits in a single BLE notification. */
    static bool fits_single_packet(const std::string& json, int mtu = 244);
};

}  // namespace orbit::comms
