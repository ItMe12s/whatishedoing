#pragma once

#include <cstdint>
#include <string>

namespace text_policy {

    std::string formatDuration(int totalSeconds);
    std::string formatDurationMs(std::int64_t totalMilliseconds);

} // namespace text_policy
