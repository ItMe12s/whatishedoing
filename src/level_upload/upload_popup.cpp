#include "message.hpp"

#include <Geode/modify/UploadPopup.hpp>

#include "embed_colors.hpp"
#include "webhook.hpp"

using namespace geode::prelude;

class $modify(UploadPopup) {
    void levelUploadFinished(GJGameLevel* level) {
        UploadPopup::levelUploadFinished(level);
        auto mod = Mod::get();
        if (!mod->getSettingValue<bool>("notify-level-upload")) return;

        bool isUpdate = level->m_levelVersion > 1;
        if (isUpdate && !mod->getSettingValue<bool>("upload-send-on-update")) return;

        std::string content = level_upload::buildUploadMessage(level, isUpdate);

        if (mod->getSettingValue<bool>("upload-use-custom-text")) {
            sendWebhookContent(content);
        } else {
            sendWebhookDirect(
                isUpdate ? "Level Updated" : "New Level Uploaded",
                content,
                isUpdate ? embed_color::kEditorExit : embed_color::kEditorOpen
            );
        }
    }
};
