#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>

#include "state.hpp"
#include "webhook.hpp"

using namespace geode::prelude;

namespace {
void sendEditorExitWebhook(std::string const& actionTitle) {
    auto& session = editorSession();
    if (!session.active) return;

    auto playerName = getPlayerName();
    auto elapsed = formatDuration(secondsSince(session.startTime));
    auto levelName = displayLevelName(session.levelName);

    sendWebhook(
        "notify-editor",
        actionTitle,
        fmt::format("{} left the editor after {}.", playerName, elapsed),
        15158332,
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

        auto levelName = displayLevelName(std::string(level->m_levelName));
        auto playerName = getPlayerName();

        auto& session = editorSession();
        session.startTime = Clock::now();
        session.levelName = levelName;
        session.active = true;

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
