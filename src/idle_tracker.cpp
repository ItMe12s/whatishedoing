#include "idle_tracker.hpp"

#include <Geode/cocos/base_nodes/CCNode.h>
#include <Geode/loader/Loader.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/CCTouchDispatcher.hpp>
#include <array>

#include "embed_colors.hpp"
#include "state.hpp"
#include "webhook.hpp"

using namespace geode::prelude;

// making this customizable next update
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
            embed_color::kIdle
        );
        idleThresholdMask() |= mask;
    }
}

// this is not it bro but i can add extra stuff later
class IdlePollNode : public cocos2d::CCNode {
public:
    static IdlePollNode* create() {
        auto* node = new IdlePollNode();
        if (node && node->init()) {
            node->autorelease();
            return node;
        }
        delete node;
        return nullptr;
    }

    void poll(float) {
        checkIdleThresholds();
    }
};
} // namespace

void registerIdlePolling() {
    Loader::get()->queueInMainThread([] {
        static bool registered = false;
        if (registered) return;

        if (!cocos2d::CCDirector::sharedDirector()) return;

        auto* node = IdlePollNode::create();
        if (!node) return;

        node->retain();
        node->schedule(schedule_selector(IdlePollNode::poll), 1.f);

        registered = true;
        log::info("Hi from idle tracker");
    });
}

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
        if (index == 0 && touches->count() > 0) {
            markActivity();
        }
        cocos2d::CCTouchDispatcher::touches(touches, event, index);
    }
};
