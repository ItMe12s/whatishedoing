#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "embed_colors.hpp"
#include "state.hpp"
#include "webhook.hpp"

using namespace geode::prelude;

namespace {
void syncPlayMode(PlayLayer* layer) {
    auto& session = levelSession();
    session.practice = layer->m_isPracticeMode;
}

void reopenLevelSessionIfNeeded(PlayLayer* layer) {
    auto& session = levelSession();
    if (session.active || !layer->m_level) return;

    auto* level = layer->m_level;
    session.levelID = level->m_levelID.value();
    session.levelName = std::string(level->m_levelName);
    session.creatorName = displayCreatorName(std::string(level->m_creatorName));
    session.accumulated = Milliseconds::zero();
    session.attemptStart = Clock::now();
    session.active = true;
    session.startPercent = static_cast<int>(level->m_normalPercent.value());
    session.bestNotifiedPercent = session.startPercent;
    syncPlayMode(layer);
}

void sendNewBestWebhookIfNeeded(GJGameLevel* level) {
    if (!Mod::get()->getSettingValue<bool>("notify-new-best")) return;

    auto& session = levelSession();
    if (!session.active || !level) return;
    if (session.levelID != level->m_levelID.value()) return;
    if (session.practice) return;

    auto currentBest = static_cast<int>(level->m_newNormalPercent2.value());
    if (currentBest <= session.bestNotifiedPercent) return;
    if (currentBest <= session.startPercent) return;
    if (currentBest >= 100) return;

    session.bestNotifiedPercent = currentBest;

    auto playerName = getPlayerName();
    auto levelName = displayLevelName(std::string(level->m_levelName));
    auto creatorName = displayCreatorName(std::string(level->m_creatorName));

    sendWebhookDirect(
        "New Best!",
        fmt::format(
            "{} reached a new best of **{}%** on **{}** by **{}**.",
            playerName,
            currentBest,
            levelName,
            creatorName
        ),
        embed_color::kNewBest,
        {
            {"Level", levelName, true},
            {"Creator", creatorName, true},
            {"Best", fmt::format("{}%", currentBest), true},
        }
    );
}
} // namespace

class $modify(MyPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        markActivity();

        auto& session = levelSession();

        auto levelName = std::string(level->m_levelName);
        auto creatorName = std::string(level->m_creatorName);
        auto creatorDisplayName = displayCreatorName(creatorName);
        auto levelID = level->m_levelID.value();
        auto displayName = displayLevelName(levelName);

        bool const isContinuation =
            session.active && session.levelID == levelID;

        if (isContinuation) {
            session.accumulated += std::chrono::duration_cast<Milliseconds>(
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
        session.startPercent = static_cast<int>(level->m_normalPercent.value());
        session.bestNotifiedPercent = session.startPercent;

        auto playerName = getPlayerName();

        if (!isContinuation) {
            sendWebhook(
                session.settingKey(),
                session.startTitle(),
                fmt::format("{} is now playing **{}** by **{}**.", playerName, displayName, creatorDisplayName),
                session.color(),
                {
                    {"Level", displayName, true},
                    {"Creator", creatorDisplayName, true},
                    {"Level ID", std::to_string(levelID), true},
                }
            );
        }

        return true;
    }

    void togglePracticeMode(bool practiceMode) {
        PlayLayer::togglePracticeMode(practiceMode);
        if (practiceMode) {
            reopenLevelSessionIfNeeded(this);
        }
        auto& session = levelSession();
        if (!session.active) return;

        syncPlayMode(this);
    }

    void levelComplete() {
        markActivity();
        syncPlayMode(this);
        auto& pre = levelSession();
        if (!pre.active) {
            PlayLayer::levelComplete();
            return;
        }
        auto elapsed = formatDurationMs(pre.elapsedMilliseconds());
        auto levelName = displayLevelName(std::string(m_level->m_levelName));
        auto creatorName = displayCreatorName(std::string(m_level->m_creatorName));
        auto playerName = getPlayerName();
        auto completeTitle = pre.completeTitle();
        auto completeColor = pre.practice ? pre.color() : embed_color::kLevelComplete;

        PlayLayer::levelComplete();

        sendNewBestWebhookIfNeeded(m_level);

        sendWebhook(
            "notify-level-complete",
            completeTitle,
            fmt::format("{} beat **{}** by **{}**!", playerName, levelName, creatorName),
            completeColor,
            {
                {"Level", levelName, true},
                {"Creator", creatorName, true},
            },
            elapsed
        );

        levelSession().reset();
    }

    void onQuit() {
        auto& session = levelSession();
        if (!session.active) {
            PlayLayer::onQuit();
            return;
        }

        markActivity();
        syncPlayMode(this);

        auto playerName = getPlayerName();
        auto elapsed = formatDurationMs(session.elapsedMilliseconds());
        auto levelName = displayLevelName(session.levelName);
        auto creatorName = displayCreatorName(session.creatorName);

        sendWebhook(
            session.settingKey(),
            session.exitTitle(),
            fmt::format("{} exited **{}** after {}.", playerName, levelName, elapsed),
            session.practice ? session.color() : embed_color::kLevelExit,
            {
                {"Level", levelName, true},
                {"Creator", creatorName, true},
            },
            elapsed
        );

        session.reset();
        PlayLayer::onQuit();
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        PlayLayer::destroyPlayer(player, object);
        markActivity();

        syncPlayMode(this);
        sendNewBestWebhookIfNeeded(m_level);
    }
};
