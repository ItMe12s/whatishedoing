#include "level_filter.hpp"

#include <cctype>
#include <charconv>

namespace level_filter {

    LevelIds parseLevelIds(std::string_view raw) {
        LevelIds ids;
        std::size_t start = 0;
        while (start < raw.size()) {
            while (start < raw.size() &&
                   (raw[start] == ',' || std::isspace(static_cast<unsigned char>(raw[start])))) {
                ++start;
            }
            if (start == raw.size()) {
                break;
            }
            std::size_t end = start;
            while (end < raw.size() && raw[end] != ',' &&
                   !std::isspace(static_cast<unsigned char>(raw[end]))) {
                ++end;
            }
            int id = 0;
            auto const result = std::from_chars(raw.data() + start, raw.data() + end, id);
            if (result.ec == std::errc{}) {
                ids.insert(id);
            }
            start = end;
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
        if (raw == "Blacklist") {
            return Mode::Blacklist;
        }
        if (raw == "Whitelist") {
            return Mode::Whitelist;
        }
        return Mode::All;
    }

    bool shouldRedact(int levelId, Mode mode, LevelIds const& ids) noexcept {
        bool const inList = levelId > 0 && ids.contains(levelId);
        return (mode == Mode::Blacklist && inList) || (mode == Mode::Whitelist && !inList);
    }

} // namespace level_filter
