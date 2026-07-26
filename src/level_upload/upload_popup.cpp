#include "embed_colors.hpp"
#include "message.hpp"
#include "webhook.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/UploadPopup.hpp>
#include <utility>

using namespace geode::prelude;

class $modify(WebhookUploadPopup, UploadPopup) {
    void levelUploadFinished(GJGameLevel* level) {
        UploadPopup::levelUploadFinished(level);
        auto mod = Mod::get();
        if (!mod->getSettingValue<bool>("notify-level-upload")) return;

        bool isUpdate = level->m_levelVersion > 1;
        if (isUpdate && !mod->getSettingValue<bool>("upload-send-on-update")) return;

        std::string content = level_upload::buildUploadMessage(level, isUpdate);

        if (mod->getSettingValue<bool>("upload-use-custom-text")) {
            sendWebhookContent(content);
        }
        else {
            sendWebhook(
                WebhookMessage{
                    .title = isUpdate ? "Level Updated" : "New Level Uploaded",
                    .description = std::move(content),
                    .color = isUpdate ? embed_color::editorExit() : embed_color::editorOpen(),
                }
            );
        }
    }
};
