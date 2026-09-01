#include "level_filter.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

namespace level_filter {

    LevelIds parseLevelIds(std::string_view raw) {
        std::string normalized(raw);
        std::ranges::replace_if(
            normalized,
            [](unsigned char c) {
                return c == ',' || std::isspace(c);
            },
            ' '
        );

        LevelIds ids;
        for (auto const part : std::views::split(normalized, ' ')) {
            std::string_view const token{part.begin(), part.end()};
            int id = 0;
            if (std::from_chars(token.data(), token.data() + token.size(), id).ec == std::errc{}) {
                ids.insert(id);
            }
        }
        return ids;
    }

    Mode parseMode(std::string_view raw) noexcept {
        return raw == "Blacklist" ? Mode::Blacklist :
            raw == "Whitelist"    ? Mode::Whitelist :
                                    Mode::All;
    }

    bool shouldRedact(int levelId, Mode mode, LevelIds const& ids) noexcept {
        bool const inList = levelId > 0 && ids.contains(levelId);
        return (mode == Mode::Blacklist && inList) || (mode == Mode::Whitelist && !inList);
    }

} // namespace level_filter
