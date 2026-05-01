#include <Geode/Geode.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/utils/general.hpp>
#include <Geode/utils/string.hpp>
#include <cvolton.level-id-api/include/EditorIDs.hpp>
#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "embed_colors.hpp"
#include "screenshot.hpp"
#include "state.hpp"
#include "webhook.hpp"

using namespace geode::prelude;

namespace {

void syncPlayMode(PlayLayer* layer) {
    auto& session = levelSession();
    session.practice = layer->m_isPracticeMode;
}

void applyStartposSegmentStart(PlayLayer* layer) {
    if (!layer || !layer->m_level) {
        return;
    }
    if (!layer->m_isTestMode) {
        return;
    }
    if (layer->m_isPracticeMode) {
        return;
    }
    auto& s = levelSession();
    s.startPercent = static_cast<int>(layer->getCurrentPercent());
    s.bestNotifiedPercent = s.startPercent;
}

void queueStartposSegmentStart(PlayLayer* layer) {
    if (!layer || !layer->m_level || !layer->m_isTestMode) {
        return;
    }
    geode::queueInMainThread([layer] {
        if (!layer->m_level || !layer->m_isTestMode) {
            return;
        }
        if (!levelSession().active) {
            return;
        }
        applyStartposSegmentStart(layer);
    });
}

int clampedScreenshotScalePercent() {
    return std::clamp(
        static_cast<int>(
            Mod::get()->getSettingValue<int64_t>("screenshot-scale-percent")
        ),
        25,
        100
    );
}

std::string screenshotEncodeTmpPath() {
    return geode::utils::string::pathToString(
        Mod::get()->getSaveDir() / "whatishedoing_cap_tmp.png"
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
    session.creatorName =
        displayCreatorName(std::string(level->m_creatorName));
    session.accumulated = Milliseconds::zero();
    session.attemptStart = Clock::now();
    session.active = true;
    session.startPercent =
        static_cast<int>(level->m_normalPercent.value());
    session.bestNotifiedPercent = session.startPercent;
    syncPlayMode(layer);
    if (layer->m_isTestMode) {
        queueStartposSegmentStart(layer);
    }
}

void sendDeathWebhookIfNeeded(
    PlayLayer* layer,
    int currentPercent,
    int bestBefore
) {
    auto& session = levelSession();
    if (!session.active || !layer || !layer->m_level) {
        return;
    }
    if (session.deathNotified) {
        return;
    }
    if (session.practice) {
        return;
    }
    if (layer->m_level->isPlatformer()) {
        return;
    }
    if (currentPercent <= 0) {
        return;
    }
    if (currentPercent >= 100) {
        return;
    }
    bool const fromStartpos = layer->m_isTestMode;
    if (!fromStartpos && currentPercent > bestBefore) {
        if (Mod::get()->getSettingValue<bool>("notify-new-best")) {
            return;
        }
    }
    if (fromStartpos) {
        auto const minSeg = static_cast<int>(Mod::get()->getSettingValue<int64_t>(
            "startpos-death-min-progress"
        ));
        int const progress = currentPercent - session.startPercent;
        if (progress < 0) {
            return;
        }
        if (progress < minSeg) {
            return;
        }
    } else {
        auto const minPct = static_cast<int>(
            Mod::get()->getSettingValue<int64_t>("death-min-percent")
        );
        if (currentPercent < minPct) {
            return;
        }
    }
    auto const playerName = getPlayerName();
    auto const display = resolveLevelDisplay(
        EditorIDs::getID(layer->m_level),
        std::string(layer->m_level->m_levelName),
        std::string(layer->m_level->m_creatorName)
    );
    if (display.redacted &&
        Mod::get()->getSettingValue<bool>("suppress-redacted")) {
        session.deathNotified = true;
        return;
    }
    int const deathStartPct = session.startPercent;
    auto sendDeath =
        [=, &session](std::optional<std::vector<std::uint8_t>> shot) {
            if (fromStartpos) {
                sendWebhookDirect(
                    "Died",
                    fmt::format(
                        "{} got a **{}-{}%** run on **{}** by **{}**.",
                        playerName,
                        deathStartPct,
                        currentPercent,
                        display.levelName,
                        display.creatorName
                    ),
                    embed_color::kDeath,
                    {
                        {"Level", display.levelName, true},
                        {"Creator", display.creatorName, true},
                        {"Run",
                         fmt::format(
                             "{}-{}%",
                             deathStartPct,
                             currentPercent
                         ),
                         true},
                    },
                    "",
                    std::move(shot)
                );
            } else {
                sendWebhookDirect(
                    "Died",
                    fmt::format(
                        "{} died at **{}%** on **{}** by **{}**.",
                        playerName,
                        currentPercent,
                        display.levelName,
                        display.creatorName
                    ),
                    embed_color::kDeath,
                    {
                        {"Level", display.levelName, true},
                        {"Creator", display.creatorName, true},
                        {"Percent", fmt::format("{}%", currentPercent), true},
                    },
                    "",
                    std::move(shot)
                );
            }
            session.deathNotified = true;
        };
    if (!Mod::get()->getSettingValue<bool>("screenshot-death")) {
        sendDeath(std::nullopt);
        return;
    }
    auto capOpt = capturePlayLayerScreenshotRgba(layer);
    if (!capOpt) {
        sendDeath(std::nullopt);
        return;
    }
    spawnScreenshotEncodeToPngThen(
        std::move(*capOpt),
        clampedScreenshotScalePercent(),
        screenshotEncodeTmpPath(),
        [=, &session](std::optional<std::vector<std::uint8_t>> shot) {
            sendDeath(std::move(shot));
        }
    );
}

void sendNewBestWebhookIfNeeded(PlayLayer* playLayer) {
    if (!Mod::get()->getSettingValue<bool>("notify-new-best")) {
        return;
    }
    if (!playLayer || !playLayer->m_level) {
        return;
    }
    if (playLayer->m_isTestMode && !playLayer->m_isPracticeMode) {
        return;
    }
    auto* level = playLayer->m_level;
    auto& session = levelSession();
    if (!session.active) {
        return;
    }
    if (session.levelID != EditorIDs::getID(level)) {
        return;
    }
    if (session.practice) {
        return;
    }
    auto const currentBest =
        static_cast<int>(level->m_newNormalPercent2.value());
    if (currentBest <= session.bestNotifiedPercent) {
        return;
    }
    if (currentBest <= session.startPercent) {
        return;
    }
    if (currentBest >= 100) {
        return;
    }
    session.bestNotifiedPercent = currentBest;
    auto const playerName = getPlayerName();
    auto const display = resolveLevelDisplay(
        EditorIDs::getID(level),
        std::string(level->m_levelName),
        std::string(level->m_creatorName)
    );
    if (display.redacted &&
        Mod::get()->getSettingValue<bool>("suppress-redacted")) {
        return;
    }
    auto sendNewBest =
        [=](std::optional<std::vector<std::uint8_t>> shot) {
            sendWebhookDirect(
                "New Best!",
                fmt::format(
                    "{} reached a new best of **{}%** on **{}** by **{}**.",
                    playerName,
                    currentBest,
                    display.levelName,
                    display.creatorName
                ),
                embed_color::kNewBest,
                {
                    {"Level", display.levelName, true},
                    {"Creator", display.creatorName, true},
                    {"Best", fmt::format("{}%", currentBest), true},
                },
                "",
                std::move(shot)
            );
        };
    if (!Mod::get()->getSettingValue<bool>("screenshot-new-best")) {
        sendNewBest(std::nullopt);
        return;
    }
    auto capOpt = capturePlayLayerScreenshotRgba(playLayer);
    if (!capOpt) {
        sendNewBest(std::nullopt);
        return;
    }
    spawnScreenshotEncodeToPngThen(
        std::move(*capOpt),
        clampedScreenshotScalePercent(),
        screenshotEncodeTmpPath(),
        [=](std::optional<std::vector<std::uint8_t>> shot) {
            sendNewBest(std::move(shot));
        }
    );
}
} // namespace

class $modify(MyPlayLayer, PlayLayer) {
    bool init(
        GJGameLevel* level,
        bool useReplay,
        bool dontCreateObjects
    ) {
        auto& session = levelSession();
        std::string const levelName =
            level ? std::string(level->m_levelName) : "";
        int const levelID = level
            ? EditorIDs::getID(level)
            : kLevelSessionClearedId;
        bool const isContinuation = level
            && session.active
            && session.levelID == levelID
            && session.levelName == levelName;
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
            session.accumulated +=
                std::chrono::duration_cast<Milliseconds>(
                    Clock::now() - session.attemptStart
                );
        } else {
            session.accumulated = Milliseconds::zero();
            session.levelID = levelID;
        }
        session.attemptStart = Clock::now();
        session.levelName = levelName;
        session.creatorName = creatorDisplayName;
        session.active = true;
        syncPlayMode(this);
        if (m_isTestMode) {
            session.startPercent =
                static_cast<int>(level->m_normalPercent.value());
            session.bestNotifiedPercent = session.startPercent;
            queueStartposSegmentStart(this);
        } else {
            session.startPercent =
                static_cast<int>(level->m_normalPercent.value());
            session.bestNotifiedPercent = session.startPercent;
        }
        auto const playerName = getPlayerName();
        if (!isContinuation) {
            auto const display = resolveLevelDisplay(
                levelID,
                levelName,
                creatorName
            );
            if (display.redacted &&
                Mod::get()->getSettingValue<bool>("suppress-redacted")) {
                return true;
            }
            std::vector<WebhookField> fields = {
                {"Level", display.levelName, true},
                {"Creator", display.creatorName, true},
            };
            if (display.showLevelID) {
                fields.push_back(
                    {"Level ID", geode::utils::numToString(levelID), true}
                );
            }
            sendWebhook(
                session.settingKey(),
                session.startTitle(),
                fmt::format(
                    "{} is now playing **{}** by **{}**.",
                    playerName,
                    display.levelName,
                    display.creatorName
                ),
                session.color(),
                fields
            );
        }
        return true;
    }
    void resetLevel() {
        PlayLayer::resetLevel();
        reopenLevelSessionIfNeeded(this);
        if (m_isTestMode && m_level) {
            queueStartposSegmentStart(this);
        }
        levelSession().deathNotified = false;
    }
    void togglePracticeMode(bool practiceMode) {
        PlayLayer::togglePracticeMode(practiceMode);
        if (practiceMode) {
            reopenLevelSessionIfNeeded(this);
        }
        auto& session = levelSession();
        if (!session.active) {
            return;
        }
        syncPlayMode(this);
    }
    void levelComplete() {
        syncPlayMode(this);
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
        auto const elapsed =
            formatDurationMs(pre.elapsedMilliseconds());
        auto const display = resolveLevelDisplay(
            EditorIDs::getID(m_level),
            std::string(m_level->m_levelName),
            std::string(m_level->m_creatorName)
        );
        auto const playerName = getPlayerName();
        auto const completeColor =
            pre.practice
                ? pre.color()
                : embed_color::kLevelComplete;
        bool const fromStartpos = m_isTestMode && !pre.practice;
        PlayLayer::levelComplete();
        sendNewBestWebhookIfNeeded(this);
        bool const suppress = display.redacted &&
            Mod::get()->getSettingValue<bool>("suppress-redacted");
        int const completeStartPercentSnapshot = pre.startPercent;
        std::string const completeTitleSnapshot = pre.completeTitle();
        if (!suppress) {
            if (fromStartpos) {
                auto const minSeg = static_cast<int>(
                    Mod::get()->getSettingValue<int64_t>(
                        "startpos-death-min-progress"
                    ));
                int const progress = 100 - completeStartPercentSnapshot;
                if (progress >= 0 && progress >= minSeg) {
                    auto fireWebhook =
                        [=](std::optional<std::vector<std::uint8_t>> shot) {
                            sendWebhook(
                                "notify-level-complete",
                                "Startpos Complete!",
                                fmt::format(
                                    "{} got a **{}-{}%** run on **{}** by "
                                    "**{}**.",
                                    playerName,
                                    completeStartPercentSnapshot,
                                    100,
                                    display.levelName,
                                    display.creatorName
                                ),
                                completeColor,
                                {
                                    {"Level", display.levelName, true},
                                    {"Creator", display.creatorName, true},
                                    {"Run",
                                     fmt::format(
                                         "{}-100%",
                                         completeStartPercentSnapshot
                                     ),
                                     true},
                                },
                                elapsed,
                                std::move(shot)
                            );
                        };
                    if (!Mod::get()->getSettingValue<bool>(
                            "screenshot-level-complete"
                        )) {
                        fireWebhook(std::nullopt);
                    } else {
                        auto capOpt =
                            capturePlayLayerScreenshotRgba(this);
                        if (!capOpt) {
                            fireWebhook(std::nullopt);
                        } else {
                            spawnScreenshotEncodeToPngThen(
                                std::move(*capOpt),
                                clampedScreenshotScalePercent(),
                                screenshotEncodeTmpPath(),
                                [=](std::optional<
                                    std::vector<std::uint8_t>> shot) {
                                    fireWebhook(std::move(shot));
                                }
                            );
                        }
                    }
                }
            } else {
                auto fireWebhook =
                    [=](std::optional<std::vector<std::uint8_t>> shot) {
                        sendWebhook(
                            "notify-level-complete",
                            completeTitleSnapshot,
                            fmt::format(
                                "{} beat **{}** by **{}**!",
                                playerName,
                                display.levelName,
                                display.creatorName
                            ),
                            completeColor,
                            {
                                {"Level", display.levelName, true},
                                {"Creator", display.creatorName, true},
                            },
                            elapsed,
                            std::move(shot)
                        );
                    };
                if (!Mod::get()->getSettingValue<bool>(
                        "screenshot-level-complete"
                    )) {
                    fireWebhook(std::nullopt);
                } else {
                    auto capOpt = capturePlayLayerScreenshotRgba(this);
                    if (!capOpt) {
                        fireWebhook(std::nullopt);
                    } else {
                        spawnScreenshotEncodeToPngThen(
                            std::move(*capOpt),
                            clampedScreenshotScalePercent(),
                            screenshotEncodeTmpPath(),
                            [=](std::optional<
                                std::vector<std::uint8_t>> shot) {
                                fireWebhook(std::move(shot));
                            }
                        );
                    }
                }
            }
        }
        levelSession().reset();
    }
    void onQuit() {
        auto& session = levelSession();
        if (!session.active) {
            PlayLayer::onQuit();
            return;
        }
        syncPlayMode(this);
        auto const playerName = getPlayerName();
        auto const elapsed =
            formatDurationMs(session.elapsedMilliseconds());
        auto const display = resolveLevelDisplay(
            session.levelID,
            session.levelName,
            session.creatorName
        );
        bool const suppress = display.redacted &&
            Mod::get()->getSettingValue<bool>("suppress-redacted");
        if (!suppress) {
            sendWebhook(
                session.settingKey(),
                session.exitTitle(),
                fmt::format(
                    "{} exited **{}**.",
                    playerName,
                    display.levelName
                ),
                session.practice
                    ? session.color()
                    : embed_color::kLevelExit,
                {
                    {"Level", display.levelName, true},
                    {"Creator", display.creatorName, true},
                },
                elapsed
            );
        }
        session.reset();
        PlayLayer::onQuit();
    }
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        bool const trackDeath =
            Mod::get()->getSettingValue<bool>("notify-death");
        int pctBefore = 0;
        int bestBefore = 0;
        if (trackDeath) {
            pctBefore =
                static_cast<int>(this->getCurrentPercent());
            bestBefore = m_level
                ? static_cast<int>(m_level->m_newNormalPercent2.value())
                : 0;
        }
        PlayLayer::destroyPlayer(player, object);
        syncPlayMode(this);
        sendNewBestWebhookIfNeeded(this);
        if (trackDeath) {
            sendDeathWebhookIfNeeded(this, pctBefore, bestBefore);
        }
    }
};
