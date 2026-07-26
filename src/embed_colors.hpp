#pragma once

#include <Geode/loader/Mod.hpp>
#include <cocos2d.h>

namespace embed_color {

    inline int fromKey(char const* key) {
        auto c = geode::Mod::get()->getSettingValue<cocos2d::ccColor3B>(key);
        return (static_cast<int>(c.r) << 16) | (static_cast<int>(c.g) << 8) | static_cast<int>(c.b);
    }

    inline int gameClose() {
        return fromKey("color-game-close");
    }

    inline int gameOpen() {
        return fromKey("color-game-open");
    }

    inline int testWebhook() {
        return fromKey("color-test-webhook");
    }

    inline int editorOpen() {
        return fromKey("color-editor-open");
    }

    inline int editorExit() {
        return fromKey("color-editor-exit");
    }

    inline int newBest() {
        return fromKey("color-new-best");
    }

    inline int levelComplete() {
        return fromKey("color-level-complete");
    }

    inline int levelExit() {
        return fromKey("color-level-exit");
    }

    inline int death() {
        return fromKey("color-death");
    }

    inline int playPractice() {
        return fromKey("color-play-practice");
    }

    inline int playNormal() {
        return fromKey("color-play-normal");
    }

} // namespace embed_color
