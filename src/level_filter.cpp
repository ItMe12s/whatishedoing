#include "level_filter.hpp"

#include <algorithm>
#include <charconv>
#include <locale>
#include <sstream>
#include <utility>

namespace level_filter {

    LevelIds parseLevelIds(std::string_view raw) {
        std::string normalized(raw);
        auto const& locale = std::locale::classic();
        std::ranges::replace_if(
            normalized,
            [&locale](char c) {
                return c == ',' || std::isspace(c, locale);
            },
            ' '
        );

        LevelIds ids;
        std::istringstream input(std::move(normalized));
        input.imbue(locale);
        for (std::string token; input >> token;) {
            int id = 0;
            auto const result = std::from_chars(token.data(), token.data() + token.size(), id);
            if (result.ec == std::errc{}) {
                ids.insert(id);
            }
        }
        return ids;
    }

    std::string serializeLevelIds(LevelIds const& ids) {
        std::string out;
        for (int id : ids) {
            if (!out.empty()) {
                out.push_back(',');
            }
            out += std::to_string(id);
        }
        return out;
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
