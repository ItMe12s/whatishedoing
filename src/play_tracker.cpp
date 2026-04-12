#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "state.hpp"
#include "webhook.hpp"

using namespace geode::prelude;

namespace {
void sendNewBestWebhookIfNeeded(GJGameLevel* level) {
    if (!level) return;
    if (!Mod::get()->getSettingValue<bool>("notify-new-best")) return;

    auto& session = levelSession();
    if (session.practice || session.testPlay) return;

    auto currentBest = static_cast<int>(level->m_newNormalPercent2.value());
    if (currentBest <= session.bestNotifiedPercent) return;
    if (currentBest <= session.startPercent) return;

    session.bestNotifiedPercent = currentBest;

    auto playerName = getPlayerName();
    auto levelName = displayLevelName(std::string(level->m_levelName));
    auto creatorName = displayCreatorName(std::string(level->m_creatorName));

    sendWebhook(
        "notify-new-best",
        "New Best!",
        fmt::format(
            "{} reached a new best of **{}%** on **{}** by **{}**.",
            playerName,
            currentBest,
            levelName,
            creatorName
        ),
        15844367,
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
        auto levelID = std::to_string(level->m_levelID.value());
        auto levelIDValue = level->m_levelID.value();
        auto displayName = displayLevelName(levelName);

        if (session.active && session.levelID == levelIDValue) {
            session.accumulated += std::chrono::duration_cast<Seconds>(
                Clock::now() - session.attemptStart
            );
        } else {
            session.accumulated = Seconds::zero();
            session.levelID = levelIDValue;
        }

        session.attemptStart = Clock::now();
        session.levelName = levelName;
        session.creatorName = creatorDisplayName;
        session.active = true;
        session.practice = m_isPracticeMode;
        session.testPlay = m_isTestMode;
        session.notifySettingKey = session.testPlay ? "notify-editor-testplay" : "notify-play-level";
        session.startPercent = static_cast<int>(level->m_normalPercent.value());
        session.bestNotifiedPercent = session.startPercent;

        auto playerName = getPlayerName();

        sendWebhook(
            session.notifyKey(),
            session.testPlay ? fmt::format("Playtesting {}", displayName) : session.startTitle(),
            fmt::format("{} is now playing **{}** by **{}**.", playerName, displayName, creatorDisplayName),
            session.color(),
            {
                {"Level", displayName, true},
                {"Creator", creatorDisplayName, true},
                {"Level ID", levelID, true},
            }
        );

        return true;
    }

    void togglePracticeMode(bool practiceMode) {
        PlayLayer::togglePracticeMode(practiceMode);
        auto& session = levelSession();
        if (!session.active) return;

        session.practice = m_isPracticeMode;
        session.testPlay = m_isTestMode;
    }

    void levelComplete() {
        PlayLayer::levelComplete();
        if (!m_level) return;

        markActivity();
        auto& session = levelSession();
        session.practice = m_isPracticeMode;
        session.testPlay = m_isTestMode;

        auto levelName = displayLevelName(std::string(m_level->m_levelName));
        auto creatorName = displayCreatorName(std::string(m_level->m_creatorName));
        auto playerName = getPlayerName();
        auto elapsed = formatDuration(session.elapsedSeconds());

        sendWebhook(
            session.testPlay ? session.notifyKey() : "notify-level-complete",
            session.completeTitle(),
            fmt::format("{} beat **{}** by **{}**!", playerName, levelName, creatorName),
            session.testPlay ? session.color() : 16766720,
            {
                {"Level", levelName, true},
                {"Creator", creatorName, true},
            },
            elapsed
        );

        session.reset();
    }

    void onQuit() {
        auto& session = levelSession();
        if (!session.active) {
            PlayLayer::onQuit();
            return;
        }

        markActivity();
        session.practice = m_isPracticeMode;
        session.testPlay = m_isTestMode;

        auto playerName = getPlayerName();
        auto elapsed = formatDuration(session.elapsedSeconds());
        auto levelName = displayLevelName(session.levelName);
        auto creatorName = displayCreatorName(session.creatorName);

        sendWebhook(
            session.notifyKey(),
            session.exitTitle(),
            fmt::format("{} exited **{}** after {}.", playerName, levelName, elapsed),
            session.testPlay ? session.color() : 15548997,
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

        auto& session = levelSession();
        session.practice = m_isPracticeMode;
        session.testPlay = m_isTestMode;
        sendNewBestWebhookIfNeeded(m_level);
    }
};
