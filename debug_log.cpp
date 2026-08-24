#include "debug_log.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace hexpuzzle {

DebugLog::DebugLog(const std::optional<std::filesystem::path>& path) {
    if (!path.has_value()) {
        return;
    }
    stream_.open(*path, std::ios::out | std::ios::app);
    if (!stream_) {
        throw std::runtime_error("cannot open debug log: " + path->string());
    }
}

bool DebugLog::enabled() const noexcept {
    return stream_.is_open();
}

void DebugLog::event(std::string_view name) {
    event(name, std::string{});
}

void DebugLog::event(std::string_view name, const std::string& message) {
    if (!enabled()) {
        return;
    }
    stream_ << "{\"schema\":\"hexpuzzle.debug_event.v1\",\"timestamp\":\""
            << timestamp() << "\",\"event\":\"" << escapeJson(name) << '"';
    if (!message.empty()) {
        stream_ << ",\"message\":\"" << escapeJson(message) << '"';
    }
    stream_ << "}\n";
    stream_.flush();
}

std::string DebugLog::escapeJson(std::string_view value) {
    std::ostringstream escaped;
    for (const unsigned char character : value) {
        switch (character) {
        case '"':
            escaped << "\\\"";
            break;
        case '\\':
            escaped << "\\\\";
            break;
        case '\b':
            escaped << "\\b";
            break;
        case '\f':
            escaped << "\\f";
            break;
        case '\n':
            escaped << "\\n";
            break;
        case '\r':
            escaped << "\\r";
            break;
        case '\t':
            escaped << "\\t";
            break;
        default:
            if (character < 0x20U) {
                escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<unsigned int>(character) << std::dec;
            } else {
                escaped << static_cast<char>(character);
            }
            break;
        }
    }
    return escaped.str();
}

std::string DebugLog::timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm utcTime{};
#if defined(_WIN32)
    gmtime_s(&utcTime, &currentTime);
#else
    gmtime_r(&currentTime, &utcTime);
#endif
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  now.time_since_epoch()) %
        std::chrono::seconds(1);
    std::ostringstream result;
    result << std::put_time(&utcTime, "%Y-%m-%dT%H:%M:%S") << '.' << std::setw(3)
           << std::setfill('0') << milliseconds.count() << 'Z';
    return result.str();
}

}  // namespace hexpuzzle
