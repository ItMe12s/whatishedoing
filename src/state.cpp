#include "embed_colors.hpp"
#include "state.hpp"

using namespace geode::prelude;

namespace {
GameSession g_gameSession;
LevelSession g_levelSession;
EditorSession g_editorSession;

Clock::time_point g_lastActivityTime = Clock::now();
uint8_t g_idleThresholdMask = 0;
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
    levelID = 0;
    levelName.clear();
    creatorName.clear();
    active = false;
    practice = false;
    startPercent = 0;
    bestNotifiedPercent = 0;
}

void EditorSession::reset() {
    active = false;
    levelName.clear();
}

GameSession& gameSession() {
    return g_gameSession;
}

LevelSession& levelSession() {
    return g_levelSession;
}

EditorSession& editorSession() {
    return g_editorSession;
}

Clock::time_point& lastActivityTime() {
    return g_lastActivityTime;
}

uint8_t& idleThresholdMask() {
    return g_idleThresholdMask;
}

void markActivity() {
    g_lastActivityTime = Clock::now();
    g_idleThresholdMask = 0;
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
