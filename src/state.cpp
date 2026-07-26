#include "state.hpp"

#include "embed_colors.hpp"
#include "level_filter.hpp"

#include <Geode/Geode.hpp>
#include <string>

using namespace geode::prelude;

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

std::string LevelSession::exitTitle() const {
    return mode == RunMode::Practice ? "Exited a Practice Run" : "Exited a Level";
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
    return creatorName.empty() ? "Unknown" : creatorName;
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
    auto const out = level_filter::serializeLevelIds(ids);
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
