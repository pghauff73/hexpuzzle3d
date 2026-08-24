#pragma once

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

namespace hexpuzzle {

class DebugLog {
public:
    DebugLog() = default;
    explicit DebugLog(const std::optional<std::filesystem::path>& path);

    DebugLog(const DebugLog&) = delete;
    DebugLog& operator=(const DebugLog&) = delete;

    bool enabled() const noexcept;
    void event(std::string_view name);
    void event(std::string_view name, const std::string& message);

private:
    static std::string escapeJson(std::string_view value);
    static std::string timestamp();

    std::ofstream stream_;
};

}  // namespace hexpuzzle
