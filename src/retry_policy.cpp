#include "retry_policy.hpp"

#include <algorithm>
#include <charconv>
#include <limits>

namespace retry_policy {

    std::optional<int> delayForFailure(
        int statusCode, std::optional<std::string_view> retryAfter, int attempt, int maxRetries,
        std::optional<int> maxDelaySeconds
    ) noexcept {
        if (attempt >= maxRetries) {
            return std::nullopt;
        }
        if (statusCode == 429) {
            if (retryAfter && !retryAfter->empty()) {
                int value = 0;
                auto const parsed = std::from_chars(
                    retryAfter->data(), retryAfter->data() + retryAfter->size(), value
                );
                if (parsed.ec == std::errc{}) {
                    int const maximum = maxDelaySeconds.value_or(86'400);
                    return std::clamp(value, 1, maximum);
                }
                return 2;
            }
            return std::min(2, maxDelaySeconds.value_or(2));
        }

        int delay = std::numeric_limits<int>::max();
        if (attempt >= 0 && std::cmp_less(attempt, std::numeric_limits<unsigned int>::digits - 1)) {
            delay = 1 << attempt;
        }
        return std::min(delay, maxDelaySeconds.value_or(delay));
    }

} // namespace retry_policy
