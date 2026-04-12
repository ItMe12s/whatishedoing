#include <Geode/modify/CCDirector.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/CCTouchDispatcher.hpp>
#include <array>

#include "state.hpp"
#include "webhook.hpp"

using namespace geode::prelude;

namespace {
constexpr std::array<int, 3> kIdleThresholdSeconds = {300, 1800, 7200};
constexpr std::array<const char*, 3> kIdleThresholdTitles = {
    "Idle for 5 Minutes",
    "Idle for 30 Minutes",
    "Idle for 2 Hours",
};

void checkIdleThresholds() {
    if (!Mod::get()->getSettingValue<bool>("notify-idle")) return;

    auto idleSeconds = secondsSince(lastActivityTime());
    auto playerName = getPlayerName();
    for (size_t i = 0; i < kIdleThresholdSeconds.size(); ++i) {
        auto mask = static_cast<uint8_t>(1 << i);
        if ((idleThresholdMask() & mask) != 0) continue;
        if (idleSeconds < kIdleThresholdSeconds[i]) continue;

        auto idleDuration = formatDuration(kIdleThresholdSeconds[i]);
        sendWebhook(
            "notify-idle",
            kIdleThresholdTitles[i],
            fmt::format("{} has been idle for {}.", playerName, idleDuration),
            9936031
        );
        idleThresholdMask() |= mask;
    }
}
} // namespace

class $modify(MyCCDirector, cocos2d::CCDirector) {
    void drawScene() {
        checkIdleThresholds();
        cocos2d::CCDirector::drawScene();
    }
};

class $modify(MyCCKeyboardDispatcher, cocos2d::CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(cocos2d::enumKeyCodes key, bool down, bool repeat, double timestamp) {
        if (down) {
            markActivity();
        }
        return cocos2d::CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, timestamp);
    }
};

class $modify(MyCCTouchDispatcher, cocos2d::CCTouchDispatcher) {
    void touches(cocos2d::CCSet* touches, cocos2d::CCEvent* event, unsigned int index) {
        if (index == 0 && touches && touches->count() > 0) {
            markActivity();
        }
        cocos2d::CCTouchDispatcher::touches(touches, event, index);
    }
};
