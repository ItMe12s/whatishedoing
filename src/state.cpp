#include "state.hpp"

using namespace geode::prelude;

namespace {
GameSession g_gameSession;
LevelSession g_levelSession;
EditorSession g_editorSession;

Clock::time_point g_lastActivityTime = Clock::now();
uint8_t g_idleThresholdMask = 0;
} // namespace

int LevelSession::elapsedSeconds() const {
    auto total = accumulated;
    if (active) {
        total += std::chrono::duration_cast<Seconds>(Clock::now() - attemptStart);
    }
    return static_cast<int>(total.count());
}

std::string const& LevelSession::notifyKey() const {
    return notifySettingKey;
}

std::string LevelSession::startTitle() const {
    if (testPlay) return "Playtesting a Level";
    if (practice) return "Playing a Level (Practice)";
    return "Playing a Level";
}

std::string LevelSession::exitTitle() const {
    if (testPlay) return "Stopped Playtesting";
    if (practice) return "Exited a Practice Run";
    return "Exited a Level";
}

std::string LevelSession::completeTitle() const {
    if (testPlay) return "Finished Playtesting!";
    if (practice) return "Practice Run Complete!";
    return "Level Complete!";
}

int LevelSession::color() const {
    if (testPlay) return 16753920;
    if (practice) return 9807270;
    return 5763719;
}

void LevelSession::reset() {
    accumulated = Seconds::zero();
    levelID = 0;
    levelName.clear();
    creatorName.clear();
    active = false;
    practice = false;
    testPlay = false;
    startPercent = 0;
    bestNotifiedPercent = 0;
    notifySettingKey = "notify-play-level";
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
        std::chrono::duration_cast<Seconds>(Clock::now() - start).count()
    );
}
