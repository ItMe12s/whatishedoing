#pragma once

#include <Geode/Geode.hpp>
#include <array>
#include <chrono>
#include <cstdint>
#include <string>

using Clock = std::chrono::steady_clock;
using Milliseconds = std::chrono::milliseconds;

enum class Tracked {
    LevelUpload,
    LevelUpdate,
    GameOpen,
    GameClose,
    LevelStart,
    LevelExit,
    LevelComplete,
    NewBest,
    EditorOpen,
    EditorExit,
    Death,
    ProfileSave,
    ProfileLoad,
    CustomTextEdit,
    TestWebhook,
    Unknown
};

inline constexpr int kLevelSessionClearedId = -67;

struct GameSession {
    Clock::time_point startTime;
    bool started = false;
    std::array<Tracked, 25> trackedEvents{};
    size_t eventCount = 0;
};

struct LevelSession {
    Clock::time_point attemptStart;
    Milliseconds accumulated{};
    int levelID = kLevelSessionClearedId;
    std::string levelName;
    std::string creatorName;
    bool active = false;
    bool practice = false;
    int startPercent = 0;
    int bestNotifiedPercent = 0;
    bool deathNotified = false;

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
    int levelID = kLevelSessionClearedId;
    std::string levelName;
    std::string creatorName;
    bool active = false;

    void reset();
};

GameSession& gameSession();
LevelSession& levelSession();
EditorSession& editorSession();

std::string formatSessionTrackedSummary(GameSession const& session);

std::string getPlayerName();
std::string displayLevelName(std::string const& levelName);
std::string displayCreatorName(std::string const& creatorName);
int secondsSince(Clock::time_point const& start);

struct LevelDisplay {
    std::string levelName;
    std::string creatorName;
    bool        showLevelID;
    bool        redacted;
};

LevelDisplay resolveLevelDisplay(
    int levelID,
    std::string const& rawLevelName,
    std::string const& rawCreatorName
);

bool isIdInFilterList(int id);
void setIdInFilterList(int id, bool inList);
