#include "embed_colors.hpp"
#include "state.hpp"
#include "text_policy.hpp"
#include "webhook.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/utils/general.hpp>
#include <cvolton.level-id-api/include/EditorIDs.hpp>
#include <utility>
#include <vector>

using namespace geode::prelude;

namespace {
    void sendEditorExitWebhook(std::string const& actionTitle) {
        levelSession().reset();
        auto& session = editorSession();
        if (!session.active) {
            return;
        }
        auto const display =
            resolveLevelDisplay(session.levelID, session.levelName, session.creatorName);
        if (isRedactionSuppressed(display)) {
            session.reset();
            return;
        }
        auto const playerName = getPlayerName();
        auto const elapsed = text_policy::formatDuration(
            static_cast<int>(session.timer.elapsed<std::chrono::seconds>())
        );
        sendWebhookIfEnabled(
            "notify-editor",
            WebhookMessage{
                .title = actionTitle,
                .description = fmt::format("{} left the editor.", playerName),
                .color = embed_color::fromKey("color-editor-exit"),
                .fields = makeLevelFields(display, session.levelID, true),
                .footer = elapsed,
            }
        );
        session.reset();
    }
} // namespace

class $modify(WebhookLevelEditorLayer, LevelEditorLayer) {
    bool init(GJGameLevel* level, bool unk) {
        if (!LevelEditorLayer::init(level, unk)) {
            return false;
        }
        if (!level) {
            editorSession().reset();
            return true;
        }
        levelSession().reset();
        auto& session = editorSession();
        session.timer.reset();
        auto const levelID = EditorIDs::getID(level);
        auto const nameRaw = std::string(level->m_levelName);
        auto const creatorRaw = std::string(level->m_creatorName);
        session.levelID = levelID;
        session.levelName = nameRaw;
        session.creatorName = displayCreatorName(creatorRaw);
        session.active = true;
        auto const display = resolveLevelDisplay(levelID, nameRaw, creatorRaw);
        if (isRedactionSuppressed(display)) {
            return true;
        }
        auto const playerName = getPlayerName();
        sendWebhookIfEnabled(
            "notify-editor",
            WebhookMessage{
                .title = "Opened the Editor",
                .description = fmt::format(
                    "{} opened the editor to work on **{}** by **{}**.",
                    playerName,
                    display.levelName,
                    display.creatorName
                ),
                .color = embed_color::fromKey("color-editor-open"),
                .fields = makeLevelFields(display, levelID, true),
            }
        );
        return true;
    }
};

class $modify(WebhookEditorPauseLayer, EditorPauseLayer) {
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
