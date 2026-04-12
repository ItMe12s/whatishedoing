#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>

#include "embed_colors.hpp"
#include "state.hpp"
#include "webhook.hpp"

using namespace geode::prelude;

namespace {
void sendEditorExitWebhook(std::string const& actionTitle) {
    levelSession().reset();

    auto& session = editorSession();
    if (!session.active) return;

    auto playerName = getPlayerName();
    auto elapsed = formatDuration(secondsSince(session.startTime));
    auto levelName = displayLevelName(session.levelName);

    sendWebhook(
        "notify-editor",
        actionTitle,
        fmt::format("{} left the editor after {}.", playerName, elapsed),
        embed_color::kEditorExit,
        {{"Level", levelName, true}},
        elapsed
    );

    session.reset();
}
} // namespace

class $modify(MyLevelEditorLayer, LevelEditorLayer) {
    bool init(GJGameLevel* level, bool unk) {
        if (!LevelEditorLayer::init(level, unk)) return false;
        markActivity();
        levelSession().reset();

        auto& session = editorSession();
        session.startTime = Clock::now();
        session.levelName = std::string(level->m_levelName);
        session.active = true;

        auto displayName = displayLevelName(session.levelName);
        auto playerName = getPlayerName();

        sendWebhook(
            "notify-editor",
            "Opened the Editor",
            fmt::format("{} opened the editor to work on **{}**.", playerName, displayName),
            embed_color::kEditorOpen
        );

        return true;
    }
};

class $modify(MyEditorPauseLayer, EditorPauseLayer) {
    void onSaveAndPlay(cocos2d::CCObject* sender) {
        markActivity();
        sendEditorExitWebhook("Save and Play");
        EditorPauseLayer::onSaveAndPlay(sender);
    }

    void onSaveAndExit(cocos2d::CCObject* sender) {
        markActivity();
        sendEditorExitWebhook("Exited the Editor");
        EditorPauseLayer::onSaveAndExit(sender);
    }

    void onExitEditor(cocos2d::CCObject* sender) {
        markActivity();
        sendEditorExitWebhook("Exited the Editor");
        EditorPauseLayer::onExitEditor(sender);
    }

    void onExitNoSave(cocos2d::CCObject* sender) {
        markActivity();
        sendEditorExitWebhook("Exited the Editor");
        EditorPauseLayer::onExitNoSave(sender);
    }
};
