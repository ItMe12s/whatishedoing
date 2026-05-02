#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/utils/general.hpp>
#include <cvolton.level-id-api/include/EditorIDs.hpp>
#include <vector>

#include "embed_colors.hpp"
#include "state.hpp"
#include "webhook.hpp"

using namespace geode::prelude;

namespace {
void sendEditorExitWebhook(std::string const& actionTitle) {
    auto& game_session = gameSession();
    if (game_session.eventCount < game_session.trackedEvents.size()) {
        game_session.trackedEvents[game_session.eventCount++] = Tracked::EditorExit;
    }
    levelSession().reset();
    auto& session = editorSession();
    if (!session.active) {
        return;
    }
    auto const display = resolveLevelDisplay(
        session.levelID,
        session.levelName,
        session.creatorName
    );
    if (display.redacted &&
        Mod::get()->getSettingValue<bool>("suppress-redacted")) {
        session.reset();
        return;
    }
    auto const playerName = getPlayerName();
    auto const elapsed =
        formatDuration(secondsSince(session.startTime));
    std::vector<WebhookField> fields = {
        {"Level", display.levelName, true},
        {"Creator", display.creatorName, true},
    };
    if (display.showLevelID) {
        fields.push_back(
            {"Level ID", geode::utils::numToString(session.levelID), true}
        );
    }
    sendWebhook(
        "notify-editor",
        actionTitle,
        fmt::format(
            "{} left the editor.",
            playerName
        ),
        embed_color::kEditorExit,
        fields,
        elapsed
    );
    session.reset();
}
} // namespace

class $modify(MyLevelEditorLayer, LevelEditorLayer) {
    bool init(GJGameLevel* level, bool unk) {
        if (!LevelEditorLayer::init(level, unk)) {
            return false;
        }
        if (!level) {
            editorSession().reset();
            return true;
        }
        levelSession().reset();
        auto& game_session = gameSession();
        if (game_session.eventCount < game_session.trackedEvents.size()) {
            game_session.trackedEvents[game_session.eventCount++] = Tracked::EditorOpen;
        }
        auto& session = editorSession();
        session.startTime = Clock::now();
        auto const levelID = EditorIDs::getID(level);
        auto const nameRaw = std::string(level->m_levelName);
        auto const creatorRaw = std::string(level->m_creatorName);
        session.levelID = levelID;
        session.levelName = nameRaw;
        session.creatorName = displayCreatorName(creatorRaw);
        session.active = true;
        auto const display = resolveLevelDisplay(
            levelID,
            nameRaw,
            creatorRaw
        );
        if (display.redacted &&
            Mod::get()->getSettingValue<bool>("suppress-redacted")) {
            return true;
        }
        auto const playerName = getPlayerName();
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
            "notify-editor",
            "Opened the Editor",
            fmt::format(
                "{} opened the editor to work on **{}** by **{}**.",
                playerName,
                display.levelName,
                display.creatorName
            ),
            embed_color::kEditorOpen,
            fields
        );
        return true;
    }
};

class $modify(MyEditorPauseLayer, EditorPauseLayer) {
    void onSaveAndPlay(cocos2d::CCObject* sender) {
        sendEditorExitWebhook("Save and Play");
        EditorPauseLayer::onSaveAndPlay(sender);
    }
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
