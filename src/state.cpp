#include "embed_colors.hpp"
#include "state.hpp"

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
