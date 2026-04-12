#include <Geode/Geode.hpp>
#include <Geode/modify/AppDelegate.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <chrono>

#include "webhook.hpp"

using namespace geode::prelude;

namespace {
using Clock = std::chrono::steady_clock;
using Seconds = std::chrono::seconds;

Clock::time_point g_gameSessionStart;
bool g_gameSessionStarted = false;

Clock::time_point g_levelAttemptStart;
Seconds g_levelAccumulated = Seconds::zero();
int g_levelSessionID = 0;
std::string g_levelSessionName;
std::string g_levelSessionCreator;
bool g_levelSessionActive = false;

Clock::time_point g_editorSessionStart;
std::string g_editorLevelName;
bool g_editorSessionActive = false;

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
    return static_cast<int>(std::chrono::duration_cast<Seconds>(Clock::now() - start).count());
}

int currentLevelSessionSeconds() {
    auto total = g_levelAccumulated;
    if (g_levelSessionActive) {
        total += std::chrono::duration_cast<Seconds>(Clock::now() - g_levelAttemptStart);
    }
    return static_cast<int>(total.count());
}

void resetLevelSession() {
    g_levelAccumulated = Seconds::zero();
    g_levelSessionID = 0;
    g_levelSessionName.clear();
    g_levelSessionCreator.clear();
    g_levelSessionActive = false;
}

void sendEditorExitWebhook(std::string const& actionTitle) {
    if (!g_editorSessionActive) return;

    auto playerName = getPlayerName();
    auto elapsed = formatDuration(secondsSince(g_editorSessionStart));
    auto levelName = displayLevelName(g_editorLevelName);

    sendWebhook(
        "notify-editor",
        actionTitle,
        fmt::format("{} left the editor after {}.", playerName, elapsed),
        15158332,
        {
            { "Level", levelName, true },
            { "Session Time", elapsed, true }
        }
    );

    g_editorSessionActive = false;
    g_editorLevelName.clear();
}
} // namespace

class $modify(MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        static bool s_sentOpen = false;
        if (!s_sentOpen) {
            s_sentOpen = true;
            g_gameSessionStart = Clock::now();
            g_gameSessionStarted = true;

            auto playerName = getPlayerName();
            sendWebhook(
                "notify-game-open",
                "Opened Geometry Dash",
                fmt::format("{} opened Geometry Dash!", playerName),
                3447003
            );
        }

        return true;
    }
};

class $modify(MyAppDelegate, AppDelegate) {
    void platformShutdown() {
        if (g_gameSessionStarted) {
            auto playerName = getPlayerName();
            auto elapsed = formatDuration(secondsSince(g_gameSessionStart));

            sendWebhook(
                "notify-game-open",
                "Closed Geometry Dash",
                fmt::format("{} exited Geometry Dash after {}.", playerName, elapsed),
                10038562,
                {
                    { "Session Time", elapsed, true }
                }
            );
        }

        AppDelegate::platformShutdown();
    }
};

class $modify(MyPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        auto levelName   = std::string(level->m_levelName);
        auto creatorName = std::string(level->m_creatorName);
        auto creatorDisplayName = displayCreatorName(creatorName);
        auto levelID     = std::to_string(level->m_levelID.value());
        auto levelIDValue = level->m_levelID.value();

        if (g_levelSessionActive && g_levelSessionID == levelIDValue) {
            g_levelAccumulated += std::chrono::duration_cast<Seconds>(Clock::now() - g_levelAttemptStart);
        } else {
            g_levelAccumulated = Seconds::zero();
            g_levelSessionID = levelIDValue;
        }

        g_levelAttemptStart = Clock::now();
        g_levelSessionName = levelName;
        g_levelSessionCreator = creatorDisplayName;
        g_levelSessionActive = true;
        auto playerName = getPlayerName();

        sendWebhook(
            "notify-play-level",
            "Playing a Level",
            fmt::format("{} is now playing **{}** by **{}**.", playerName, displayLevelName(levelName), creatorDisplayName),
            5763719,
            {
                { "Level", displayLevelName(levelName), true },
                { "Creator", creatorDisplayName, true },
                { "Level ID", levelID, true }
            }
        );

        return true;
    }

    void levelComplete() {
        PlayLayer::levelComplete();

        if (!m_level) return;

        auto levelName = displayLevelName(std::string(m_level->m_levelName));
        auto creatorName = displayCreatorName(std::string(m_level->m_creatorName));
        auto playerName = getPlayerName();
        auto elapsed = formatDuration(currentLevelSessionSeconds());

        sendWebhook(
            "notify-level-complete",
            "Level Complete!",
            fmt::format("{} beat **{}** by **{}**!", playerName, levelName, creatorName),
            16766720,
            {
                { "Level", levelName, true },
                { "Creator", creatorName, true },
                { "Session Time", elapsed, true }
            }
        );

        resetLevelSession();
    }

    void onQuit() {
        if (!g_levelSessionActive) {
            PlayLayer::onQuit();
            return;
        }

        auto playerName = getPlayerName();
        auto elapsed = formatDuration(currentLevelSessionSeconds());
        auto levelName = displayLevelName(g_levelSessionName);
        auto creatorName = displayCreatorName(g_levelSessionCreator);

        sendWebhook(
            "notify-play-level",
            "Exited a Level",
            fmt::format("{} exited **{}** after {}.", playerName, levelName, elapsed),
            15548997,
            {
                { "Level", levelName, true },
                { "Creator", creatorName, true },
                { "Session Time", elapsed, true }
            }
        );

        resetLevelSession();
        PlayLayer::onQuit();
    }
};

class $modify(MyLevelEditorLayer, LevelEditorLayer) {
    bool init(GJGameLevel* level, bool unk) {
        if (!LevelEditorLayer::init(level, unk)) return false;

        auto levelName = displayLevelName(std::string(level->m_levelName));
        auto playerName = getPlayerName();
        g_editorSessionStart = Clock::now();
        g_editorLevelName = levelName;
        g_editorSessionActive = true;

        sendWebhook(
            "notify-editor",
            "Opened the Editor",
            fmt::format("{} opened the editor to work on **{}**.", playerName, levelName),
            15105570
        );

        return true;
    }
};

class $modify(MyEditorPauseLayer, EditorPauseLayer) {
    void onSaveAndExit(cocos2d::CCObject* sender) {
        sendEditorExitWebhook("Exited the Editor");
        EditorPauseLayer::onSaveAndExit(sender);
    }

    void onExitEditor(cocos2d::CCObject* sender) {
        sendEditorExitWebhook("Exited the Editor");
        EditorPauseLayer::onExitEditor(sender);
    }

    void onExitNoSave(cocos2d::CCObject* sender) {
        sendEditorExitWebhook("Exited the Editor");
        EditorPauseLayer::onExitNoSave(sender);
    }
};
