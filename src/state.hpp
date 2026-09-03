#pragma once

#include "difficulty_face.hpp"
#include "play_policy.hpp"
#include "webhook.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

#include <Geode/utils/timer.hpp>

using Clock = std::chrono::steady_clock;
using Milliseconds = std::chrono::milliseconds;

inline constexpr int kLevelSessionClearedId = -67;

struct GameSession {
    geode::utils::Timer<Clock> timer;
    bool started = false;
};

struct LevelSession {
    geode::utils::Timer<Clock> attemptTimer;
    Milliseconds accumulated{};
    int levelID = kLevelSessionClearedId;
    std::string levelName;
    std::string creatorName;
    bool active = false;
    RunMode mode = RunMode::Normal;
    int startPercent = 0;
    int bestNotifiedPercent = 0;
    bool deathNotified = false;
    int difficulty = -1;
    int demonDifficulty = 0;
    int stars = 0;
    int rating = 0;

    int64_t elapsedMilliseconds() const;
    std::string startTitle() const;
    std::string completeTitle() const;
    int color() const;
    void reset();
};

struct EditorSession {
    geode::utils::Timer<Clock> timer;
    int levelID = kLevelSessionClearedId;
    std::string levelName;
    std::string creatorName;
    bool active = false;

    void reset();
};

GameSession& gameSession();
LevelSession& levelSession();
EditorSession& editorSession();

std::string getPlayerName();
class GJGameLevel;
int getLevelRating(GJGameLevel* level);
std::string displayLevelName(std::string const& levelName);
std::string displayCreatorName(std::string const& creatorName);

struct LevelDisplay {
    std::string levelName;
    std::string creatorName;
    bool showLevelID;
    bool redacted;
};

LevelDisplay resolveLevelDisplay(
    int levelID, std::string const& rawLevelName, std::string const& rawCreatorName
);

std::string levelIdLine(LevelDisplay const& display, int levelID);
bool isRedactionSuppressed(LevelDisplay const& display);

bool isIdInFilterList(int id);
void setIdInFilterList(int id, bool inList);
