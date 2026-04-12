#include <Geode/Geode.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/loader/GameEvent.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/CCTouchDispatcher.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <array>
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
bool g_levelSessionPractice = false;
bool g_levelSessionTestPlay = false;
int g_levelSessionStartPercent = 0;
int g_levelSessionBestNotifiedPercent = 0;
std::string g_levelSessionNotifyKey = "notify-play-level";

Clock::time_point g_editorSessionStart;
std::string g_editorLevelName;
bool g_editorSessionActive = false;

Clock::time_point g_lastActivityTime = Clock::now();
uint8_t g_idleThresholdMask = 0;
constexpr std::array<int, 4> kIdleThresholdSeconds = {30, 120, 900, 3600};
constexpr std::array<const char *, 4> kIdleThresholdTitles = {
    "Idle for 30 Seconds",
    "Idle for 2 Minutes",
    "Idle for 15 Minutes",
    "Idle for 1 Hour",
};

std::string getPlayerName();
int secondsSince(Clock::time_point const &start);

void markActivity() {
  g_lastActivityTime = Clock::now();
  g_idleThresholdMask = 0;
}

void checkIdleThresholds() {
  if (!Mod::get()->getSettingValue<bool>("notify-idle"))
    return;

  auto idleSeconds = secondsSince(g_lastActivityTime);
  auto playerName = getPlayerName();
  for (size_t i = 0; i < kIdleThresholdSeconds.size(); ++i) {
    auto mask = static_cast<uint8_t>(1 << i);
    if ((g_idleThresholdMask & mask) != 0)
      continue;
    if (idleSeconds < kIdleThresholdSeconds[i])
      continue;

    auto idleDuration = formatDuration(kIdleThresholdSeconds[i]);
    sendWebhook(
        "notify-idle", kIdleThresholdTitles[i],
        fmt::format("{} has been idle for {}.", playerName, idleDuration),
        9936031);
    g_idleThresholdMask |= mask;
  }
}

std::string getPlayerName() {
  auto name = Mod::get()->getSettingValue<std::string>("player-name");
  return name.empty() ? "He" : name;
}

std::string displayLevelName(std::string const &levelName) {
  return levelName.empty() ? "a level" : levelName;
}

std::string displayCreatorName(std::string const &creatorName) {
  return creatorName.empty() ? "Unknown" : creatorName;
}

int secondsSince(Clock::time_point const &start) {
  return static_cast<int>(
      std::chrono::duration_cast<Seconds>(Clock::now() - start).count());
}

int currentLevelSessionSeconds() {
  auto total = g_levelAccumulated;
  if (g_levelSessionActive) {
    total +=
        std::chrono::duration_cast<Seconds>(Clock::now() - g_levelAttemptStart);
  }
  return static_cast<int>(total.count());
}

std::string const &currentLevelNotifyKey() { return g_levelSessionNotifyKey; }

std::string currentPlayStartTitle() {
  if (g_levelSessionTestPlay)
    return "Playtesting a Level";
  if (g_levelSessionPractice)
    return "Playing a Level (Practice)";
  return "Playing a Level";
}

std::string currentPlayExitTitle() {
  if (g_levelSessionTestPlay)
    return "Stopped Playtesting";
  if (g_levelSessionPractice)
    return "Exited a Practice Run";
  return "Exited a Level";
}

std::string currentCompleteTitle() {
  if (g_levelSessionTestPlay)
    return "Finished Playtesting!";
  if (g_levelSessionPractice)
    return "Practice Run Complete!";
  return "Level Complete!";
}

int currentPlayColor() {
  if (g_levelSessionTestPlay)
    return 16753920;
  if (g_levelSessionPractice)
    return 9807270;
  return 5763719;
}

void resetLevelSession() {
  g_levelAccumulated = Seconds::zero();
  g_levelSessionID = 0;
  g_levelSessionName.clear();
  g_levelSessionCreator.clear();
  g_levelSessionActive = false;
  g_levelSessionPractice = false;
  g_levelSessionTestPlay = false;
  g_levelSessionStartPercent = 0;
  g_levelSessionBestNotifiedPercent = 0;
  g_levelSessionNotifyKey = "notify-play-level";
}

void sendNewBestWebhookIfNeeded(GJGameLevel *level) {
  if (!level)
    return;
  if (!Mod::get()->getSettingValue<bool>("notify-new-best"))
    return;
  if (g_levelSessionPractice || g_levelSessionTestPlay)
    return;

  auto currentBest = static_cast<int>(level->m_newNormalPercent2.value());
  if (currentBest <= g_levelSessionBestNotifiedPercent)
    return;
  if (currentBest <= g_levelSessionStartPercent)
    return;

  g_levelSessionBestNotifiedPercent = currentBest;

  auto playerName = getPlayerName();
  auto levelName = displayLevelName(std::string(level->m_levelName));
  auto creatorName = displayCreatorName(std::string(level->m_creatorName));

  sendWebhook(
      "notify-new-best", "New Best!",
      fmt::format("{} reached a new best of **{}%** on **{}** by **{}**.",
                  playerName, currentBest, levelName, creatorName),
      15844367,
      {{"Level", levelName, true},
       {"Creator", creatorName, true},
       {"Best", fmt::format("{}%", currentBest), true}});
}

void sendEditorExitWebhook(std::string const &actionTitle) {
  if (!g_editorSessionActive)
    return;

  auto playerName = getPlayerName();
  auto elapsed = formatDuration(secondsSince(g_editorSessionStart));
  auto levelName = displayLevelName(g_editorLevelName);

  sendWebhook("notify-editor", actionTitle,
              fmt::format("{} left the editor after {}.", playerName, elapsed),
              15158332, {{"Level", levelName, true}}, elapsed);

  g_editorSessionActive = false;
  g_editorLevelName.clear();
}
} // namespace

$execute {
  markActivity();
  GameEvent(GameEventType::Exiting)
      .listen(
          [] {
            if (!g_gameSessionStarted)
              return;

            auto playerName = getPlayerName();
            auto elapsed = formatDuration(secondsSince(g_gameSessionStart));

            sendWebhook("notify-game-open", "Closed Geometry Dash",
                        fmt::format("{} exited Geometry Dash after {}.",
                                    playerName, elapsed),
                        10038562, {}, elapsed);
          },
          0)
      .leak();
}

$on_mod(Loaded) {
  listenForSettingChanges<bool>("test-webhook", [](bool enabled) {
    if (!enabled)
      return;

    auto playerName = getPlayerName();
    sendWebhookDirect("Test Webhook",
                      fmt::format("{} is testing the webhook!", playerName),
                      3066993);
    Mod::get()->setSettingValue<bool>("test-webhook", false);
  });
}

class $modify(MenuLayer) {
  bool init() {
    if (!MenuLayer::init())
      return false;
    markActivity();

    static bool s_sentOpen = false;
    if (!s_sentOpen) {
      s_sentOpen = true;
      g_gameSessionStart = Clock::now();
      g_gameSessionStarted = true;

      auto playerName = getPlayerName();
      sendWebhook("notify-game-open", "Opened Geometry Dash",
                  fmt::format("{} opened Geometry Dash!", playerName), 3447003);
    }

    return true;
  }
};

class $modify(MyPlayLayer, PlayLayer) {
  bool init(GJGameLevel *level, bool useReplay, bool dontCreateObjects) {
    if (!PlayLayer::init(level, useReplay, dontCreateObjects))
      return false;
    markActivity();

    auto levelName = std::string(level->m_levelName);
    auto creatorName = std::string(level->m_creatorName);
    auto creatorDisplayName = displayCreatorName(creatorName);
    auto levelID = std::to_string(level->m_levelID.value());
    auto levelIDValue = level->m_levelID.value();
    auto displayName = displayLevelName(levelName);

    if (g_levelSessionActive && g_levelSessionID == levelIDValue) {
      g_levelAccumulated += std::chrono::duration_cast<Seconds>(
          Clock::now() - g_levelAttemptStart);
    } else {
      g_levelAccumulated = Seconds::zero();
      g_levelSessionID = levelIDValue;
    }

    g_levelAttemptStart = Clock::now();
    g_levelSessionName = levelName;
    g_levelSessionCreator = creatorDisplayName;
    g_levelSessionActive = true;
    g_levelSessionPractice = m_isPracticeMode;
    g_levelSessionTestPlay = m_isTestMode;
    g_levelSessionNotifyKey =
        g_levelSessionTestPlay ? "notify-editor-testplay" : "notify-play-level";
    g_levelSessionStartPercent =
        static_cast<int>(level->m_normalPercent.value());
    g_levelSessionBestNotifiedPercent = g_levelSessionStartPercent;
    auto playerName = getPlayerName();

    sendWebhook(currentLevelNotifyKey(),
                g_levelSessionTestPlay
                    ? fmt::format("Playtesting {}", displayName)
                    : currentPlayStartTitle(),
                fmt::format("{} is now playing **{}** by **{}**.", playerName,
                            displayName, creatorDisplayName),
                currentPlayColor(),
                {{"Level", displayName, true},
                 {"Creator", creatorDisplayName, true},
                 {"Level ID", levelID, true}});

    return true;
  }

  void levelComplete() {
    PlayLayer::levelComplete();

    if (!m_level)
      return;
    markActivity();

    auto levelName = displayLevelName(std::string(m_level->m_levelName));
    auto creatorName = displayCreatorName(std::string(m_level->m_creatorName));
    auto playerName = getPlayerName();
    auto elapsed = formatDuration(currentLevelSessionSeconds());
    sendWebhook(g_levelSessionTestPlay ? currentLevelNotifyKey()
                                       : "notify-level-complete",
                currentCompleteTitle(),
                fmt::format("{} beat **{}** by **{}**!", playerName, levelName,
                            creatorName),
                g_levelSessionTestPlay ? currentPlayColor() : 16766720,
                {{"Level", levelName, true}, {"Creator", creatorName, true}},
                elapsed);

    resetLevelSession();
  }

  void onQuit() {
    if (!g_levelSessionActive) {
      PlayLayer::onQuit();
      return;
    }
    markActivity();

    auto playerName = getPlayerName();
    auto elapsed = formatDuration(currentLevelSessionSeconds());
    auto levelName = displayLevelName(g_levelSessionName);
    auto creatorName = displayCreatorName(g_levelSessionCreator);

    sendWebhook(currentLevelNotifyKey(), currentPlayExitTitle(),
                fmt::format("{} exited **{}** after {}.", playerName, levelName,
                            elapsed),
                g_levelSessionTestPlay ? currentPlayColor() : 15548997,
                {{"Level", levelName, true}, {"Creator", creatorName, true}},
                elapsed);

    resetLevelSession();
    PlayLayer::onQuit();
  }

  void destroyPlayer(PlayerObject *player, GameObject *object) {
    PlayLayer::destroyPlayer(player, object);
    markActivity();
    sendNewBestWebhookIfNeeded(m_level);
  }
};

class $modify(MyLevelEditorLayer, LevelEditorLayer) {
  bool init(GJGameLevel *level, bool unk) {
    if (!LevelEditorLayer::init(level, unk))
      return false;
    markActivity();

    auto levelName = displayLevelName(std::string(level->m_levelName));
    auto playerName = getPlayerName();
    g_editorSessionStart = Clock::now();
    g_editorLevelName = levelName;
    g_editorSessionActive = true;

    sendWebhook("notify-editor", "Opened the Editor",
                fmt::format("{} opened the editor to work on **{}**.",
                            playerName, levelName),
                15105570);

    return true;
  }
};

class $modify(MyCCDirector, cocos2d::CCDirector) {
  void drawScene() {
    checkIdleThresholds();
    cocos2d::CCDirector::drawScene();
  }
};

class $modify(MyCCKeyboardDispatcher, cocos2d::CCKeyboardDispatcher) {
  void dispatchKeyboardMSG(cocos2d::enumKeyCodes key, bool down, bool repeat) {
    if (down) {
      markActivity();
    }
    cocos2d::CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat);
  }
};

class $modify(MyCCTouchDispatcher, cocos2d::CCTouchDispatcher) {
  void touches(cocos2d::CCSet *touches, cocos2d::CCEvent *event,
               unsigned int index) {
    if (index == 0 && touches && touches->count() > 0) {
      markActivity();
    }
    cocos2d::CCTouchDispatcher::touches(touches, event, index);
  }
};

class $modify(MyEditorPauseLayer, EditorPauseLayer) {
  void onSaveAndExit(cocos2d::CCObject *sender) {
    markActivity();
    sendEditorExitWebhook("Exited the Editor");
    EditorPauseLayer::onSaveAndExit(sender);
  }

  void onExitEditor(cocos2d::CCObject *sender) {
    markActivity();
    sendEditorExitWebhook("Exited the Editor");
    EditorPauseLayer::onExitEditor(sender);
  }

  void onExitNoSave(cocos2d::CCObject *sender) {
    markActivity();
    sendEditorExitWebhook("Exited the Editor");
    EditorPauseLayer::onExitNoSave(sender);
  }
};
