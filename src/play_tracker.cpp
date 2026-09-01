#include "embed_colors.hpp"
#include "play_events.hpp"
#include "screenshot.hpp"
#include "state.hpp"
#include "text_policy.hpp"
#include "webhook.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/EndLevelLayer.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <cmath>
#include <cstdint>
#include <cvolton.level-id-api/include/EditorIDs.hpp>
#include <deque>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace geode::prelude;

class $modify(WebhookPlayLayer, PlayLayer) {
    struct Fields {
        bool noclip = false;
        bool speedhack = false;
        CCObject* disabledCheat = nullptr;
        std::optional<Clock::time_point> speedhackCompare;
        std::deque<double> realTimeHistory;
        std::deque<double> gameTimeHistory;
        double rollingRealSum = 0;
        double rollingGameSum = 0;
        double currentTimeWarp = 1;
        std::uint64_t screenshotEpoch = 0;
    };

    static void onModify(auto& self) {
        (void)self.setHookPriorityPre("PlayLayer::destroyPlayer", Priority::First);
    }

    void resetSpeedhackSamples() {
        m_fields->realTimeHistory.clear();
        m_fields->gameTimeHistory.clear();
        m_fields->rollingRealSum = 0;
        m_fields->rollingGameSum = 0;
        m_fields->speedhackCompare = std::nullopt;
    }

    void clearCheatState() {
        m_fields->noclip = false;
        m_fields->speedhack = false;
        m_fields->disabledCheat = nullptr;
        resetSpeedhackSamples();
    }

    bool isProgressLegal() {
        if (!Mod::get()->getSettingValue<bool>("cheat-detect")) {
            return true;
        }
        return !m_fields->noclip && !m_isIgnoreDamageEnabled && !m_ignoreDamage &&
            !m_fields->speedhack;
    }

    void checkSpeedhackDelta(float dt) {
        if (!Mod::get()->getSettingValue<bool>("cheat-detect")) {
            resetSpeedhackSamples();
            return;
        }
        if (!levelSession().active || m_levelEndAnimationStarted) {
            resetSpeedhackSamples();
            return;
        }
        if (!m_player1 || m_player1->m_isDead || m_isPaused) {
            return;
        }
        auto const now = Clock::now();
        if (!m_fields->speedhackCompare.has_value()) {
            m_fields->speedhackCompare = now;
            return;
        }
        std::chrono::duration<double> const realElapsed = now - m_fields->speedhackCompare.value();
        m_fields->speedhackCompare = now;
        double const realDt = realElapsed.count();
        if (realDt > 0.2) {
            return;
        }
        double const gameDt = static_cast<double>(dt);
        if (realDt <= 0 || gameDt <= 0) {
            return;
        }
        m_fields->rollingRealSum += realDt;
        m_fields->rollingGameSum += gameDt;
        m_fields->realTimeHistory.push_back(realDt);
        m_fields->gameTimeHistory.push_back(gameDt);
        constexpr std::size_t kMaxSamples = 120;
        if (m_fields->realTimeHistory.size() > kMaxSamples) {
            m_fields->rollingRealSum -= m_fields->realTimeHistory.front();
            m_fields->rollingGameSum -= m_fields->gameTimeHistory.front();
            m_fields->realTimeHistory.pop_front();
            m_fields->gameTimeHistory.pop_front();
        }
        if (m_fields->realTimeHistory.size() < 30 || m_fields->rollingRealSum == 0) {
            return;
        }
        double const currentRatio = m_fields->rollingGameSum / m_fields->rollingRealSum;
        double const expectedRatio = m_fields->currentTimeWarp;
        if (std::abs(currentRatio - expectedRatio) > 0.05) {
            if (!m_fields->speedhack) {
                log::warn("Speedhack detected");
                m_fields->speedhack = true;
            }
        }
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        play_events::clearCompletedLevelExit(nullptr);
        auto& session = levelSession();
        std::string const levelName = level ? std::string(level->m_levelName) : "";
        int const levelID = level ? EditorIDs::getID(level) : kLevelSessionClearedId;
        bool const isContinuation =
            level && session.active && session.levelID == levelID && session.levelName == levelName;
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }
        if (!level) {
            levelSession().reset();
            return true;
        }
        auto const creatorName = std::string(level->m_creatorName);
        auto const creatorDisplayName = displayCreatorName(creatorName);
        if (isContinuation) {
            session.accumulated += Milliseconds(session.attemptTimer.elapsed<Milliseconds>());
        }
        else {
            session.accumulated = Milliseconds::zero();
            session.levelID = levelID;
        }
        session.attemptTimer.reset();
        session.levelName = levelName;
        session.creatorName = creatorDisplayName;
        session.active = true;
        play_events::syncPlayMode(this);
        session.startPercent = static_cast<int>(level->m_normalPercent.value());
        session.bestNotifiedPercent = static_cast<int>(level->m_newNormalPercent2.value());
        if (play_policy::shouldCaptureStartposSegment(session.mode)) {
            play_events::queueStartposSegmentStart(this);
        }
        auto const playerName = getPlayerName();
        if (!isContinuation) {
            auto const display = resolveLevelDisplay(levelID, levelName, creatorName);
            if (isRedactionSuppressed(display)) {
                return true;
            }
            sendWebhookIfEnabled(
                "notify-play-level",
                WebhookMessage{
                    .title = session.startTitle(),
                    .description = fmt::format(
                        "{} is now playing **{}** by **{}**.",
                        playerName,
                        display.levelName,
                        display.creatorName
                    ),
                    .color = session.color(),
                    .fields = makeLevelFields(display, levelID, true),
                }
            );
        }
        return true;
    }

    void resetLevel() {
        ++m_fields->screenshotEpoch;
        resetSpeedhackSamples();
        PlayLayer::resetLevel();
        play_events::reopenLevelSessionIfNeeded(this);
        if (m_level &&
            play_policy::shouldCaptureStartposSegment(deriveRunMode(m_isPracticeMode, m_isTestMode))) {
            play_events::queueStartposSegmentStart(this);
        }
        levelSession().deathNotified = false;
        clearCheatState();
    }

    void togglePracticeMode(bool practiceMode) {
        PlayLayer::togglePracticeMode(practiceMode);
        if (practiceMode) {
            play_events::reopenLevelSessionIfNeeded(this);
        }
        auto& session = levelSession();
        if (!session.active) {
            return;
        }
        play_events::syncPlayMode(this);
    }

    void levelComplete() {
        play_events::syncPlayMode(this);
        auto& pre = levelSession();
        if (!pre.active) {
            PlayLayer::levelComplete();
            return;
        }
        if (!m_level) {
            PlayLayer::levelComplete();
            pre.reset();
            return;
        }
        auto const elapsedMs = pre.elapsedMilliseconds();
        auto const elapsed = text_policy::formatDurationMs(elapsedMs);
        auto const display = resolveLevelDisplay(
            EditorIDs::getID(m_level),
            std::string(m_level->m_levelName),
            std::string(m_level->m_creatorName)
        );
        auto const playerName = getPlayerName();
        RunMode const completeModeSnapshot = pre.mode;
        auto const completeColor = completeModeSnapshot == RunMode::Practice ?
            pre.color() :
            embed_color::fromKey("color-level-complete");
        bool const fromStartpos = completeModeSnapshot == RunMode::Startpos;
        int const completeStartPercentSnapshot = pre.startPercent;
        int const sessionLevelID = pre.levelID;
        std::string const sessionLevelName = pre.levelName;
        auto const sessionAttemptStart = pre.attemptTimer.time();
        std::string const completeTitleSnapshot = pre.completeTitle();
        PlayLayer::levelComplete();
        auto const screenshotEpoch = ++m_fields->screenshotEpoch;
        auto const captureStillValid = [this, screenshotEpoch] {
            return m_fields->screenshotEpoch == screenshotEpoch;
        };
        bool const progressLegal = isProgressLegal();
        if (progressLegal) {
            play_events::sendNewBestWebhookIfNeeded(this, -1, -1, captureStillValid);
        }
        else {
            play_events::markCurrentBestHandled(this);
        }
        bool const suppress = isRedactionSuppressed(display);
        if (!suppress) {
            play_events::queueCompletedLevelExit(this, elapsedMs);
        }
        if (!suppress && progressLegal) {
            if (fromStartpos) {
                geode::queueInMainThread([=, layer = WeakRef<PlayLayer>(this)] {
                    auto lockedLayer = layer.lock();
                    bool const sameSession = play_events::matchesLevelSession(
                        sessionLevelID, sessionLevelName, sessionAttemptStart
                    );
                    int const startPercent = completeStartPercentSnapshot;
                    auto const minSeg = static_cast<int>(
                        Mod::get()->getSettingValue<int64_t>("startpos-death-min-progress")
                    );
                    if (play_policy::segmentMeetsThreshold(startPercent, 100, minSeg)) {
                        auto fireWebhook = [=](std::optional<std::vector<std::uint8_t>> shot) {
                            sendWebhookIfEnabled(
                                "notify-level-complete",
                                WebhookMessage{
                                    .title = "Startpos Complete!",
                                    .description = fmt::format(
                                        "{} got a **{}-{}%** run on "
                                        "**{}** by **{}**.",
                                        playerName,
                                        startPercent,
                                        100,
                                        display.levelName,
                                        display.creatorName
                                    ),
                                    .color = completeColor,
                                    .fields =
                                        {
                                            {"Level", display.levelName, true},
                                            {"Creator", display.creatorName, true},
                                            {"Run", fmt::format("{}-100%", startPercent), true},
                                        },
                                    .footer = elapsed,
                                    .screenshotPng = std::move(shot),
                                }
                            );
                        };
                        play_events::sendWithOptionalScreenshot(
                            "screenshot-level-complete",
                            lockedLayer.data(),
                            captureStillValid,
                            std::move(fireWebhook)
                        );
                    }
                    if (sameSession) {
                        levelSession().reset();
                    }
                });
                clearCheatState();
                return;
            }
            else {
                auto fireWebhook = [=](std::optional<std::vector<std::uint8_t>> shot) {
                    sendWebhookIfEnabled(
                        completeModeSnapshot == RunMode::Practice ? "notify-practice-complete" :
                                                                    "notify-level-complete",
                        WebhookMessage{
                            .title = completeTitleSnapshot,
                            .description = fmt::format(
                                "{} beat **{}** by **{}**!", playerName, display.levelName, display.creatorName
                            ),
                            .color = completeColor,
                            .fields = makeLevelFields(display, sessionLevelID, false),
                            .footer = elapsed,
                            .screenshotPng = std::move(shot),
                        }
                    );
                };
                play_events::sendWithOptionalScreenshot(
                    "screenshot-level-complete", this, captureStillValid, std::move(fireWebhook)
                );
            }
        }
        clearCheatState();
        levelSession().reset();
    }

    void onQuit() {
        ++m_fields->screenshotEpoch;
        auto& session = levelSession();
        if (!session.active) {
            play_events::clearCompletedLevelExit(this);
            PlayLayer::onQuit();
            return;
        }
        if (play_events::consumeSentCompletedLevelExit(this)) {
            PlayLayer::onQuit();
            return;
        }
        play_events::syncPlayMode(this);
        auto const playerName = getPlayerName();
        auto const display =
            resolveLevelDisplay(session.levelID, session.levelName, session.creatorName);
        if (!isRedactionSuppressed(display)) {
            play_events::sendLevelExitWebhook(
                session.mode,
                display,
                session.levelID,
                playerName,
                text_policy::formatDurationMs(session.elapsedMilliseconds())
            );
        }
        session.reset();
        play_events::clearCompletedLevelExit(this);
        PlayLayer::onQuit();
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        bool const trackDeath = Mod::get()->getSettingValue<bool>("notify-death");
        int const pctBefore = static_cast<int>(this->getCurrentPercent());
        int const bestBefore = m_level ? static_cast<int>(m_level->m_newNormalPercent2.value()) : 0;
        PlayLayer::destroyPlayer(player, object);
        auto const screenshotEpoch = m_fields->screenshotEpoch;
        auto const captureStillValid = [this, screenshotEpoch] {
            return m_fields->screenshotEpoch == screenshotEpoch;
        };
        resetSpeedhackSamples();
        if (!m_fields->disabledCheat) {
            m_fields->disabledCheat = object;
        }
        if (!m_fields->noclip && m_fields->disabledCheat != object && player && !player->m_isDead &&
            !m_levelEndAnimationStarted) {
            log::warn("Noclip detected");
            m_fields->noclip = true;
        }
        play_events::syncPlayMode(this);
        bool const progressLegal = isProgressLegal();
        if (progressLegal && pctBefore > 0) {
            play_events::sendNewBestWebhookIfNeeded(this, pctBefore, bestBefore, captureStillValid);
        }
        else if (pctBefore > 0) {
            play_events::markCurrentBestHandled(this, pctBefore, bestBefore);
        }
        if (trackDeath && progressLegal) {
            play_events::sendDeathWebhookIfNeeded(this, pctBefore, bestBefore, captureStillValid);
        }
    }

    void postUpdate(float dt) {
        checkSpeedhackDelta(dt);
        PlayLayer::postUpdate(dt);
    }

    void updateTimeWarp(float timeWarp) {
        PlayLayer::updateTimeWarp(timeWarp);
        m_fields->currentTimeWarp = timeWarp;
        resetSpeedhackSamples();
    }
};

class $modify(WebhookEndLevelLayer, EndLevelLayer) {
    void onMenu(CCObject* sender) {
        play_events::sendCompletedLevelExitIfQueued(m_playLayer);
        EndLevelLayer::onMenu(sender);
    }

    void onReplay(CCObject* sender) {
        play_events::clearCompletedLevelExit(m_playLayer);
        EndLevelLayer::onReplay(sender);
    }

    void onRestartCheckpoint(CCObject* sender) {
        play_events::clearCompletedLevelExit(m_playLayer);
        EndLevelLayer::onRestartCheckpoint(sender);
    }
};
