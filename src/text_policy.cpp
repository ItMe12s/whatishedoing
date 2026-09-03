#include "text_policy.hpp"

#include <fmt/format.h>
#include <string>
#include <vector>

namespace text_policy {

    std::string formatDuration(int totalSeconds) {
        int const hours = totalSeconds / 3600;
        int const minutes = (totalSeconds % 3600) / 60;
        int const seconds = totalSeconds % 60;
        auto unit = [](int count, char const* name) {
            return fmt::format("{} {}{}", count, name, count == 1 ? "" : "s");
        };

        std::vector<std::string> parts;
        if (hours) {
            parts.push_back(unit(hours, "hour"));
        }
        if (minutes) {
            parts.push_back(unit(minutes, "minute"));
        }
        if (seconds || parts.empty()) {
            parts.push_back(unit(seconds, "second"));
        }
        if (parts.size() == 1) {
            return parts.front();
        }
        if (parts.size() == 2) {
            return fmt::format("{} and {}", parts[0], parts[1]);
        }
        return fmt::format("{}, {} and {}", parts[0], parts[1], parts[2]);
    }

    std::string formatDurationMs(std::int64_t totalMilliseconds) {
        totalMilliseconds = std::max<std::int64_t>(totalMilliseconds, 0);
        if (totalMilliseconds == 0) {
            return "0 seconds";
        }
        if (totalMilliseconds < 1000) {
            return fmt::format("{:.2f} seconds", static_cast<double>(totalMilliseconds) / 1000.0);
        }
        return formatDuration(static_cast<int>(totalMilliseconds / 1000));
    }

} // namespace text_policy
