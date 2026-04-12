#include <Geode/Geode.hpp>
#include <Geode/loader/GameEvent.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include "state.hpp"
#include "webhook.hpp"

using namespace geode::prelude;

$execute {
  markActivity();
  GameEvent(GameEventType::Exiting)
      .listen(
          [] {
            auto &session = gameSession();
            if (!session.started)
              return;

            auto playerName = getPlayerName();
            auto elapsed = formatDuration(secondsSince(session.startTime));

            sendWebhook("notify-game-open", "Closed Geometry Dash",
                        fmt::format("{} exited Geometry Dash after {}.",
                                    playerName, elapsed),
                        10038562, {}, elapsed);
          },
          0)
      .leak();
}

$on_mod(Loaded) {
  listenForSettingChanges<bool>("test-webhook", [](bool enabled) {
    if (!enabled)
      return;

    auto playerName = getPlayerName();
    sendWebhookDirect("Test Webhook",
                      fmt::format("{} is testing the webhook!", playerName),
                      3066993);
    Mod::get()->setSettingValue<bool>("test-webhook", false);
  });
}

class $modify(MenuLayer) {
  bool init() {
    if (!MenuLayer::init())
      return false;
    markActivity();

    auto &session = gameSession();
    if (!session.started) {
      session.started = true;
      session.startTime = Clock::now();

      auto playerName = getPlayerName();
      sendWebhook("notify-game-open", "Opened Geometry Dash",
                  fmt::format("{} opened Geometry Dash!", playerName), 3447003);
    }

    return true;
  }
};
