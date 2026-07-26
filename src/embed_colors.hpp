#pragma once

#include <Geode/loader/Mod.hpp>
#include <cocos2d.h>

namespace embed_color {

    inline int fromKey(char const* key) {
        auto c = geode::Mod::get()->getSettingValue<cocos2d::ccColor3B>(key);
        return (static_cast<int>(c.r) << 16) | (static_cast<int>(c.g) << 8) | static_cast<int>(c.b);
    }

} // namespace embed_color
