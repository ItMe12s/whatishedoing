#include <Geode/Geode.hpp>
#include <Geode/loader/GameEvent.hpp>

#include "embed_colors.hpp"
#include "state.hpp"
#include "webhook.hpp"

using namespace geode::prelude;

$execute
{
  GameEvent(GameEventType::Loaded)
      .listen(
          []
          {
            auto &session = gameSession();
            if (session.started)
              return;

            session.started = true;
            session.startTime = Clock::now();

            auto playerName = getPlayerName();
            sendWebhook("notify-game-session", "Opened Geometry Dash",
                        fmt::format("{} opened Geometry Dash!", playerName),
                        embed_color::kGameOpen);
          })
      .leak();

  GameEvent(GameEventType::Exiting)
      .listen(
          []
          {
            auto &session = gameSession();
            if (!session.started)
              return;

            auto playerName = getPlayerName();
            auto elapsed = formatDuration(secondsSince(session.startTime));

            sendWebhook("notify-game-session", "Closed Geometry Dash",
                        fmt::format("{} exited Geometry Dash after {}.",
                                    playerName, elapsed),
                        embed_color::kGameClose, {}, elapsed);
          })
      .leak();
}

$on_mod(Loaded)
{
  listenForSettingChanges<bool>("test-webhook", [](bool enabled)
                                {
    if (!enabled)
      return;

    auto playerName = getPlayerName();
    sendWebhookDirect("Test Webhook",
                      fmt::format("{} is testing the webhook!", playerName),
                      embed_color::kTestWebhook);
    Mod::get()->setSettingValue<bool>("test-webhook", false); });
}
