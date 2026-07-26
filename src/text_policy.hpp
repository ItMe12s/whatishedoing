#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace text_policy {

    std::string clampUtf8ByBytes(std::string_view text, std::size_t maxBytes);
    std::string formatDuration(int totalSeconds);
    std::string formatDurationMs(std::int64_t totalMilliseconds);

} // namespace text_policy
