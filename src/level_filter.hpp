#pragma once

#include <set>
#include <string>
#include <string_view>

namespace level_filter {

    using LevelIds = std::set<int>;

    enum class Mode {
        All,
        Blacklist,
        Whitelist,
    };

    LevelIds parseLevelIds(std::string_view raw);
    std::string serializeLevelIds(LevelIds const& ids);
    Mode parseMode(std::string_view raw) noexcept;
    bool shouldRedact(int levelId, Mode mode, LevelIds const& ids) noexcept;

} // namespace level_filter
