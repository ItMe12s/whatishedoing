#pragma once

#include <optional>
#include <string_view>

namespace retry_policy {

    std::optional<int> delayForFailure(
        int statusCode, std::optional<std::string_view> retryAfter, int attempt, int maxRetries,
        std::optional<int> maxDelaySeconds = std::nullopt
    ) noexcept;

} // namespace retry_policy
