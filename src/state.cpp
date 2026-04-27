#include "embed_colors.hpp"
#include "state.hpp"

#include <Geode/utils/general.hpp>
#include <Geode/utils/string.hpp>
#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>
#include <vector>

using namespace geode::prelude;

namespace {
GameSession s_gameSession;
LevelSession s_levelSession;
EditorSession s_editorSession;
} // namespace

int64_t LevelSession::elapsedMilliseconds() const {
    auto total = accumulated;
    if (active) {
        total += std::chrono::duration_cast<Milliseconds>(Clock::now() - attemptStart);
    }
    return total.count();
}

std::string LevelSession::settingKey() const {
    return "notify-play-level";
}

std::string LevelSession::startTitle() const {
    if (practice) return "Playing a Level (Practice)";
    return "Playing a Level";
}

std::string LevelSession::exitTitle() const {
    if (practice) return "Exited a Practice Run";
    return "Exited a Level";
}

std::string LevelSession::completeTitle() const {
    if (practice) return "Practice Run Complete!";
    return "Level Complete!";
}

int LevelSession::color() const {
    if (practice) return embed_color::kPlayPractice;
    return embed_color::kPlayNormal;
}

void LevelSession::reset() {
    accumulated = Milliseconds::zero();
    levelID = kLevelSessionClearedId;
    levelName.clear();
    creatorName.clear();
    active = false;
    practice = false;
    startPercent = 0;
    bestNotifiedPercent = 0;
    deathNotified = false;
}

void EditorSession::reset() {
    active = false;
    levelName.clear();
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

int secondsSince(Clock::time_point const& start) {
    return static_cast<int>(
        std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - start).count()
    );
}

namespace {

constexpr char const* kRedactedLevelName = "Private level";
constexpr char const* kRedactedCreatorName = "-";

std::unordered_set<int> parseLevelIDs(std::string const& raw) {
    std::unordered_set<int> out;
    std::string normalized = raw;
    for (auto& c : normalized) {
        if (std::isspace(static_cast<unsigned char>(c))) c = ',';
    }
    for (auto const& tok : geode::utils::string::split(normalized, ",")) {
        if (tok.empty()) continue;
        auto res = geode::utils::numFromString<int>(tok);
        if (res.isOk()) {
            out.insert(res.unwrap());
        }
    }
    return out;
}

} // namespace

bool isIdInFilterList(int id) {
    if (id <= 0) {
        return false;
    }
    auto const raw =
        Mod::get()->getSettingValue<std::string>("level-filter-ids");
    return parseLevelIDs(raw).contains(id);
}

void setIdInFilterList(int id, bool inList) {
    if (id <= 0) {
        return;
    }
    auto const raw =
        Mod::get()->getSettingValue<std::string>("level-filter-ids");
    auto ids = parseLevelIDs(raw);
    if (inList) {
        ids.insert(id);
    } else {
        ids.erase(id);
    }
    std::vector<int> sorted(ids.begin(), ids.end());
    std::sort(sorted.begin(), sorted.end());
    std::string out;
    for (size_t i = 0; i < sorted.size(); ++i) {
        if (i > 0) {
            out += ',';
        }
        out += std::to_string(sorted[i]);
    }
    Mod::get()->setSettingValue<std::string>("level-filter-ids", out);
}

LevelDisplay resolveLevelDisplay(
    int levelID,
    std::string const& rawLevelName,
    std::string const& rawCreatorName
) {
    LevelDisplay normal{
        displayLevelName(rawLevelName),
        displayCreatorName(rawCreatorName),
        levelID > 0,
        false
    };
    auto const mode =
        Mod::get()->getSettingValue<std::string>("level-filter-mode");
    if (mode != "Blacklist" && mode != "Whitelist") {
        return normal;
    }
    auto const idsRaw =
        Mod::get()->getSettingValue<std::string>("level-filter-ids");
    auto const ids = parseLevelIDs(idsRaw);
    bool const inList = levelID > 0 && ids.contains(levelID);
    bool const redact =
        (mode == "Blacklist" && inList) ||
        (mode == "Whitelist" && !inList);
    if (!redact) {
        return normal;
    }
    return LevelDisplay{kRedactedLevelName, kRedactedCreatorName, false, true};
}
