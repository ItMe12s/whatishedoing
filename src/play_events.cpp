#include "play_events.hpp"

#include "embed_colors.hpp"
#include "screenshot.hpp"
#include "state.hpp"
#include "text_policy.hpp"
#include "webhook.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/utils/general.hpp>
#include <cstdint>
#include <cvolton.level-id-api/include/EditorIDs.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace geode::prelude;

namespace play_events {
    namespace {

        struct PendingCompletedLevelExit {
            WeakRef<PlayLayer> layer;
            int levelID = kLevelSessionClearedId;
            std::string levelName;
            std::string creatorName;
            RunMode mode = RunMode::Normal;
            std::int64_t elapsedMs = 0;
            Clock::time_point attemptStart;
        };

        std::optional<PendingCompletedLevelExit> s_pendingCompletedLevelExit;
        std::optional<PendingCompletedLevelExit> s_sentCompletedLevelExit;

    } // namespace

    void syncPlayMode(PlayLayer* layer) {
        auto& session = levelSession();
        session.mode = deriveRunMode(layer->m_isPracticeMode, layer->m_isTestMode);
    }

    bool matchesLevelSession(int levelID, std::string const& levelName, Clock::time_point attemptStart) {
        auto const& session = levelSession();
        return session.active && session.levelID == levelID && session.levelName == levelName &&
            session.attemptTimer.time() == attemptStart;
    }

    void queueStartposSegmentStart(PlayLayer* layer) {
        if (!layer || !layer->m_level ||
            !play_policy::shouldCaptureStartposSegment(
                deriveRunMode(layer->m_isPracticeMode, layer->m_isTestMode)
            )) {
            return;
        }
        auto const levelID = EditorIDs::getID(layer->m_level);
        std::string const levelName = std::string(layer->m_level->m_levelName);
        auto const attemptStart = levelSession().attemptTimer.time();
        int const startPercent = static_cast<int>(layer->getCurrentPercent());
        geode::queueInMainThread(
            [layer = WeakRef(layer), levelID, levelName, attemptStart, startPercent] {
                auto activeLayer = layer.lock();
                if (!activeLayer || PlayLayer::get() != activeLayer.data() || !activeLayer->m_level) {
                    return;
                }
                if (!play_policy::shouldCaptureStartposSegment(
                        deriveRunMode(activeLayer->m_isPracticeMode, activeLayer->m_isTestMode)
                    )) {
                    return;
                }
                if (!matchesLevelSession(levelID, levelName, attemptStart)) {
                    return;
                }
                levelSession().startPercent = startPercent;
            }
        );
    }

    void reopenLevelSessionIfNeeded(PlayLayer* layer) {
        auto& session = levelSession();
        if (session.active || !layer->m_level) {
            return;
        }
        auto* level = layer->m_level;
        session.levelID = EditorIDs::getID(level);
        session.levelName = std::string(level->m_levelName);
        session.creatorName = displayCreatorName(std::string(level->m_creatorName));
        session.accumulated = Milliseconds::zero();
        session.attemptTimer.reset();
        session.active = true;
        session.startPercent = static_cast<int>(level->m_normalPercent.value());
        session.bestNotifiedPercent = static_cast<int>(level->m_newNormalPercent2.value());
        syncPlayMode(layer);
        if (play_policy::shouldCaptureStartposSegment(session.mode)) {
            queueStartposSegmentStart(layer);
        }
    }

    void sendDeathWebhookIfNeeded(
        PlayLayer* layer, int currentPercent, int bestBefore, geode::Function<bool()> captureStillValid
    ) {
        auto& session = levelSession();
        if (!layer || !layer->m_level) {
            return;
        }
        auto const normalMinimum =
            static_cast<int>(Mod::get()->getSettingValue<int64_t>("death-min-percent"));
        auto const startposMinimum =
            static_cast<int>(Mod::get()->getSettingValue<int64_t>("startpos-death-min-progress"));
        if (!play_policy::shouldNotifyDeath(
                play_policy::DeathPolicy{
                    .active = session.active,
                    .alreadyNotified = session.deathNotified,
                    .progressLegal = true,
                    .platformer = layer->m_level->isPlatformer(),
                    .mode = session.mode,
                    .currentPercent = currentPercent,
                    .bestBefore = bestBefore,
                    .notifyNewBest = Mod::get()->getSettingValue<bool>("notify-new-best"),
                    .startPercent = session.startPercent,
                    .normalMinimumPercent = normalMinimum,
                    .startposMinimumProgress = startposMinimum,
                }
            )) {
            return;
        }
        bool const fromStartpos = session.mode == RunMode::Startpos;
        auto const playerName = getPlayerName();
        auto const display = resolveLevelDisplay(
            EditorIDs::getID(layer->m_level),
            std::string(layer->m_level->m_levelName),
            std::string(layer->m_level->m_creatorName)
        );
        if (display.redacted && Mod::get()->getSettingValue<bool>("suppress-redacted")) {
            session.deathNotified = true;
            return;
        }
        int const deathStartPercent = session.startPercent;
        int const sessionLevelID = session.levelID;
        std::string const sessionLevelName = session.levelName;
        auto const sessionAttemptStart = session.attemptTimer.time();
        session.deathNotified = true;
        auto sendDeath = [=](std::optional<std::vector<std::uint8_t>> screenshot) {
            if (fromStartpos) {
                sendWebhook(
                    WebhookMessage{
                        .title = "Died",
                        .description = fmt::format(
                            "{} got a **{}-{}%** run on **{}** by **{}**.",
                            playerName,
                            deathStartPercent,
                            currentPercent,
                            display.levelName,
                            display.creatorName
                        ),
                        .color = embed_color::fromKey("color-death"),
                        .fields =
                            {
                                {"Level", display.levelName, true},
                                {"Creator", display.creatorName, true},
                                {"Run", fmt::format("{}-{}%", deathStartPercent, currentPercent), true},
                            },
                        .screenshotPng = std::move(screenshot),
                    }
                );
            }
            else {
                sendWebhook(
                    WebhookMessage{
                        .title = "Died",
                        .description = fmt::format(
                            "{} died at **{}%** on **{}** by **{}**.",
                            playerName,
                            currentPercent,
                            display.levelName,
                            display.creatorName
                        ),
                        .color = embed_color::fromKey("color-death"),
                        .fields =
                            {
                                {"Level", display.levelName, true},
                                {"Creator", display.creatorName, true},
                                {"Percent", fmt::format("{}%", currentPercent), true},
                            },
                        .screenshotPng = std::move(screenshot),
                    }
                );
            }
        };
        if (!Mod::get()->getSettingValue<bool>("screenshot-death")) {
            sendDeath(std::nullopt);
            return;
        }
        capturePlayLayerScreenshotAfterDelay(
            layer,
            [captureStillValid = std::move(captureStillValid),
             sessionLevelID,
             sessionLevelName,
             sessionAttemptStart]() mutable {
                return captureStillValid() &&
                    matchesLevelSession(sessionLevelID, sessionLevelName, sessionAttemptStart);
            },
            std::move(sendDeath)
        );
    }

    void sendNewBestWebhookIfNeeded(
        PlayLayer* layer, int percentAtDeath, int bestBeforeDeath,
        geode::Function<bool()> captureStillValid
    ) {
        if (!layer || !layer->m_level) {
            return;
        }
        auto* level = layer->m_level;
        auto& session = levelSession();
        int const storedBest = static_cast<int>(level->m_newNormalPercent2.value());
        auto const minimumPercent =
            static_cast<int>(Mod::get()->getSettingValue<int64_t>("new-best-min-percent"));
        auto const display = resolveLevelDisplay(
            EditorIDs::getID(level), std::string(level->m_levelName), std::string(level->m_creatorName)
        );
        auto const best = play_policy::newBestToNotify(
            play_policy::NewBestPolicy{
                .enabled = Mod::get()->getSettingValue<bool>("notify-new-best"),
                .active = session.active,
                .sameLevel = session.levelID == EditorIDs::getID(level),
                .progressLegal = true,
                .mode = session.mode,
                .startPercent = session.startPercent,
                .bestNotifiedPercent = session.bestNotifiedPercent,
                .storedBest = storedBest,
                .percentAtDeath = percentAtDeath,
                .bestBeforeDeath = bestBeforeDeath,
                .minimumPercent = minimumPercent,
                .redacted = display.redacted,
                .suppressRedacted = Mod::get()->getSettingValue<bool>("suppress-redacted"),
            }
        );
        if (!best) {
            return;
        }
        int const effectiveBest = *best;
        auto const playerName = getPlayerName();
        session.bestNotifiedPercent = effectiveBest;
        int const sessionLevelID = session.levelID;
        std::string const sessionLevelName = session.levelName;
        auto const sessionAttemptStart = session.attemptTimer.time();
        auto sendNewBest = [=](std::optional<std::vector<std::uint8_t>> screenshot) {
            sendWebhook(
                WebhookMessage{
                    .title = "New Best!",
                    .description = fmt::format(
                        "{} reached a new best of **{}%** on **{}** by **{}**.",
                        playerName,
                        effectiveBest,
                        display.levelName,
                        display.creatorName
                    ),
                    .color = embed_color::fromKey("color-new-best"),
                    .fields =
                        {
                            {"Level", display.levelName, true},
                            {"Creator", display.creatorName, true},
                            {"Best", fmt::format("{}%", effectiveBest), true},
                        },
                    .screenshotPng = std::move(screenshot),
                }
            );
        };
        if (!Mod::get()->getSettingValue<bool>("screenshot-new-best")) {
            sendNewBest(std::nullopt);
            return;
        }
        capturePlayLayerScreenshotAfterDelay(
            layer,
            [captureStillValid = std::move(captureStillValid),
             sessionLevelID,
             sessionLevelName,
             sessionAttemptStart]() mutable {
                return captureStillValid() &&
                    matchesLevelSession(sessionLevelID, sessionLevelName, sessionAttemptStart);
            },
            std::move(sendNewBest)
        );
    }

    void clearCompletedLevelExit(PlayLayer* layer) {
        if (s_pendingCompletedLevelExit) {
            auto pendingLayer = s_pendingCompletedLevelExit->layer.lock();
            if (!pendingLayer || pendingLayer.data() == layer) {
                s_pendingCompletedLevelExit.reset();
            }
        }
        if (s_sentCompletedLevelExit) {
            auto sentLayer = s_sentCompletedLevelExit->layer.lock();
            if (!sentLayer || sentLayer.data() == layer) {
                s_sentCompletedLevelExit.reset();
            }
        }
    }

    void queueCompletedLevelExit(PlayLayer* layer, std::int64_t elapsedMilliseconds) {
        auto const& session = levelSession();
        s_pendingCompletedLevelExit = PendingCompletedLevelExit{
            layer,
            session.levelID,
            session.levelName,
            session.creatorName,
            session.mode,
            elapsedMilliseconds,
            session.attemptTimer.time(),
        };
    }

    void sendCompletedLevelExitIfQueued(PlayLayer* layer) {
        if (!s_pendingCompletedLevelExit) {
            return;
        }
        auto pendingLayer = s_pendingCompletedLevelExit->layer.lock();
        if (!pendingLayer || pendingLayer.data() != layer) {
            s_pendingCompletedLevelExit.reset();
            return;
        }
        auto pending = std::move(*s_pendingCompletedLevelExit);
        s_pendingCompletedLevelExit.reset();
        s_sentCompletedLevelExit = pending;
        auto const display =
            resolveLevelDisplay(pending.levelID, pending.levelName, pending.creatorName);
        bool const suppress =
            display.redacted && Mod::get()->getSettingValue<bool>("suppress-redacted");
        if (suppress) {
            return;
        }
        auto const playerName = getPlayerName();
        sendWebhookIfEnabled(
            "notify-play-level",
            WebhookMessage{
                .title = pending.mode == RunMode::Practice ? "Exited a Practice Run" : "Exited a Level",
                .description = fmt::format("{} exited **{}**.", playerName, display.levelName),
                .color = pending.mode == RunMode::Practice ?
                    embed_color::fromKey("color-play-practice") :
                    embed_color::fromKey("color-level-exit"),
                .fields =
                    {
                        {"Level", display.levelName, true},
                        {"Creator", display.creatorName, true},
                    },
                .footer = text_policy::formatDurationMs(pending.elapsedMs),
            }
        );
    }

    bool consumeSentCompletedLevelExit(PlayLayer* layer) {
        if (!s_sentCompletedLevelExit) {
            return false;
        }
        auto sentLayer = s_sentCompletedLevelExit->layer.lock();
        if (!sentLayer || sentLayer.data() != layer) {
            s_sentCompletedLevelExit.reset();
            return false;
        }
        auto const& sent = *s_sentCompletedLevelExit;
        if (!matchesLevelSession(sent.levelID, sent.levelName, sent.attemptStart)) {
            return false;
        }
        s_sentCompletedLevelExit.reset();
        levelSession().reset();
        return true;
    }

    void markCurrentBestHandled(PlayLayer* layer, int percentAtDeath, int bestBeforeDeath) {
        if (!layer || !layer->m_level) {
            return;
        }
        auto* level = layer->m_level;
        auto& session = levelSession();
        if (layer->m_isTestMode && !layer->m_isPracticeMode && session.startPercent > 0) {
            return;
        }
        if (!session.active) {
            return;
        }
        if (session.levelID != EditorIDs::getID(level)) {
            return;
        }
        if (session.mode == RunMode::Practice) {
            return;
        }
        int const storedBest = static_cast<int>(level->m_newNormalPercent2.value());
        int const effectiveBest =
            play_policy::effectiveBest(storedBest, percentAtDeath, bestBeforeDeath);
        if (effectiveBest > session.bestNotifiedPercent) {
            session.bestNotifiedPercent = effectiveBest;
        }
    }

} // namespace play_events
