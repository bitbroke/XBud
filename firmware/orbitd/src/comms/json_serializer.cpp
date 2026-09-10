/**
 * @file json_serializer.cpp
 */

#include "comms/json_serializer.h"
#include <sstream>

namespace orbit::comms {

std::string JsonSerializer::escape(const std::string& input) {
    std::ostringstream ss;
    for (char c : input) {
        switch (c) {
            case '"':  ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b";  break;
            case '\f': ss << "\\f";  break;
            case '\n': ss << "\\n";  break;
            case '\r': ss << "\\r";  break;
            case '\t': ss << "\\t";  break;
            default:   ss << c;      break;
        }
    }
    return ss.str();
}

bool JsonSerializer::fits_single_packet(const std::string& json, int mtu) {
    return json.size() <= static_cast<size_t>(mtu);
}

}  // namespace orbit::comms
