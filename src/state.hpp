#pragma once

#include <Geode/Geode.hpp>
#include <chrono>
#include <cstdint>
#include <string>

using Clock = std::chrono::steady_clock;
using Milliseconds = std::chrono::milliseconds;

struct GameSession {
    Clock::time_point startTime;
    bool started = false;
};

struct LevelSession {
    Clock::time_point attemptStart;
    Milliseconds accumulated{};
    int levelID = 0;
    std::string levelName;
    std::string creatorName;
    bool active = false;
    bool practice = false;
    int startPercent = 0;
    int bestNotifiedPercent = 0;

    int64_t elapsedMilliseconds() const;
    std::string settingKey() const;
    std::string startTitle() const;
    std::string exitTitle() const;
    std::string completeTitle() const;
    int color() const;
    void reset();
};

struct EditorSession {
    Clock::time_point startTime;
    std::string levelName;
    bool active = false;

    void reset();
};

GameSession& gameSession();
LevelSession& levelSession();
EditorSession& editorSession();

Clock::time_point& lastActivityTime();
uint8_t& idleThresholdMask();

void markActivity();
std::string getPlayerName();
std::string displayLevelName(std::string const& levelName);
std::string displayCreatorName(std::string const& creatorName);
int secondsSince(Clock::time_point const& start);
