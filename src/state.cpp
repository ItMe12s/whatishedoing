#include "state.hpp"

#include "embed_colors.hpp"
#include "level_filter.hpp"

#include <Geode/Enums.hpp>
#include <Geode/Geode.hpp>
#include <Geode/utils/general.hpp>
#include <Geode/utils/ranges.hpp>
#include <string>

using namespace geode::prelude;

namespace emojis {
    inline constexpr char const* Unrated = "<:Unrated:1544707376073154581>"; // NA
    inline constexpr char const* Auto = "<:Auto:1544707348931813397>";
    inline constexpr char const* Easy = "<:Easy:1544707318673834135>";
    inline constexpr char const* Normal = "<:Normal:1544707289662095401>";
    inline constexpr char const* Hard = "<:Hard:1544707258900811816>";
    inline constexpr char const* Harder = "<:Harder:1544707231176728658>";
    inline constexpr char const* Insane = "<:Insane:1544707201870991513>";
    inline constexpr char const* Demon = "<:Demon:1544707104999481384>"; // Hard Demon
    inline constexpr char const* EasyDemon = "<:EasyDemon:1544707161932824656>";
    inline constexpr char const* MediumDemon = "<:MediumDemon:1544707136037191730>";
    inline constexpr char const* InsaneDemon = "<:InsaneDemon:1544707073215045723>";
    inline constexpr char const* ExtremeDemon = "<:ExtremeDemon:1544707036699295754>";
    inline constexpr char const* Unknown = "<:glungus:1493755426590687333>";
} // namespace emojis

// Genius at work
// Look, okay, I already tried using level thumbnail mod's code but I made a bigger mess so just
// have this for now. Also it's working as intended. If you want to make it better, please do.
std::string getDifficultyEmoji(int difficulty, int demonDifficulty, int stars) {
    // log::info("difficulty={}, demonDifficulty={}, stars={}", difficulty, demonDifficulty, stars);
    if (stars <= 0) {
        return emojis::Unrated;
    }
    if (difficulty == 6) {
        // log::info("Robtop level demonDifficulty={}", demonDifficulty);
        switch (demonDifficulty) {
            case 0: return emojis::Demon;
            case 3: return emojis::EasyDemon;
            case 4: return emojis::MediumDemon;
            case 5: return emojis::InsaneDemon;
            case 6: return emojis::ExtremeDemon;
            default: return emojis::Unknown;
        }
    }
    // Online levels difficulty=0 but valid data
    if ((difficulty == 0 || difficulty == -1) && stars > 0) {
        if (stars != 10) {
            if (stars == 1) return emojis::Auto;
            if (stars == 2) return emojis::Easy;
            if (stars == 3) return emojis::Normal;
            if (stars <= 5) return emojis::Hard;
            if (stars <= 7) return emojis::Harder;
            if (stars <= 9) return emojis::Insane;
            if (stars >= 11) return emojis::Unknown; // Wtf Robtop
            return emojis::Unknown;
        }
        switch (demonDifficulty) {
            case 0: return emojis::Demon;
            case 3: return emojis::EasyDemon;
            case 4: return emojis::MediumDemon;
            case 5: return emojis::InsaneDemon;
            case 6: return emojis::ExtremeDemon;
            default: return emojis::Unknown;
        }
    }
    // GJDifficulty enum for Robtop levels
    switch (difficulty) {
        case -1: return emojis::Unrated;
        case 0: return emojis::Auto;
        case 1: return emojis::Easy;
        case 2: return emojis::Normal;
        case 3: return emojis::Hard;
        case 4: return emojis::Harder;
        case 5: return emojis::Insane;
        case 6: return emojis::Demon;
        case 7: return emojis::EasyDemon;
        case 8: return emojis::MediumDemon;
        case 9: return emojis::InsaneDemon;
        case 10: return emojis::ExtremeDemon;
        default: return emojis::Unknown;
    }
}

namespace {
    GameSession s_gameSession;
    LevelSession s_levelSession;
    EditorSession s_editorSession;
} // namespace

int64_t LevelSession::elapsedMilliseconds() const {
    return accumulated.count() + (active ? attemptTimer.elapsed<Milliseconds>() : 0);
}

std::string LevelSession::startTitle() const {
    return mode == RunMode::Practice ? "Playing a Level (Practice)" : "Playing a Level";
}

std::string LevelSession::completeTitle() const {
    return mode == RunMode::Practice ? "Practice Run Complete!" : "Level Complete!";
}

int LevelSession::color() const {
    return embed_color::fromKey(
        mode == RunMode::Practice ? "color-play-practice" : "color-play-normal"
    );
}

void LevelSession::reset() {
    *this = {};
}

void EditorSession::reset() {
    *this = {};
}

GameSession& gameSession() {
    return s_gameSession;
}

LevelSession& levelSession() {
    return s_levelSession;
}

EditorSession& editorSession() {
    return s_editorSession;
}

std::string getPlayerName() {
    auto name = Mod::get()->getSettingValue<std::string>("player-name");
    return name.empty() ? "He" : name;
}

std::string displayLevelName(std::string const& levelName) {
    return levelName.empty() ? "a level" : levelName;
}

std::string displayCreatorName(std::string const& creatorName) {
    return creatorName.empty() ? "-" : creatorName;
}

namespace {

    constexpr char const* kRedactedLevelName = "Private level";
    constexpr char const* kRedactedCreatorName = "-";

} // namespace

bool isIdInFilterList(int id) {
    if (id <= 0) {
        return false;
    }
    auto const raw = Mod::get()->getSettingValue<std::string>("level-filter-ids");
    return level_filter::parseLevelIds(raw).contains(id);
}

void setIdInFilterList(int id, bool inList) {
    if (id <= 0) {
        return;
    }
    auto const raw = Mod::get()->getSettingValue<std::string>("level-filter-ids");
    auto ids = level_filter::parseLevelIds(raw);
    if (inList) {
        ids.insert(id);
    }
    else {
        ids.erase(id);
    }
    auto const out = geode::utils::ranges::join(ids, std::string(","), [](int value) {
        return geode::utils::numToString(value);
    });
    Mod::get()->setSettingValue<std::string>("level-filter-ids", out);
}

LevelDisplay resolveLevelDisplay(
    int levelID, std::string const& rawLevelName, std::string const& rawCreatorName
) {
    LevelDisplay normal{
        displayLevelName(rawLevelName), displayCreatorName(rawCreatorName), levelID > 0, false
    };
    auto const mode =
        level_filter::parseMode(Mod::get()->getSettingValue<std::string>("level-filter-mode"));
    if (mode == level_filter::Mode::All) {
        return normal;
    }
    auto const idsRaw = Mod::get()->getSettingValue<std::string>("level-filter-ids");
    auto const ids = level_filter::parseLevelIds(idsRaw);
    bool const redact = level_filter::shouldRedact(levelID, mode, ids);
    if (!redact) {
        return normal;
    }
    return LevelDisplay{kRedactedLevelName, kRedactedCreatorName, false, true};
}

std::string levelIdLine(LevelDisplay const& display, int levelID) {
    return display.showLevelID ? fmt::format("\n-# Level ID: {}", levelID) : "";
}

bool isRedactionSuppressed(LevelDisplay const& display) {
    return display.redacted && Mod::get()->getSettingValue<bool>("suppress-redacted");
}
