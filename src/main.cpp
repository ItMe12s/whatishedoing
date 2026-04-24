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
            [] {
                auto& session = gameSession();
                if (session.started) {
                    return;
                }
                session.started = true;
                session.startTime = Clock::now();
                auto const playerName = getPlayerName();
                sendWebhook(
                    "notify-game-session",
                    "Opened Geometry Dash",
                    fmt::format(
                        "{} opened Geometry Dash!",
                        playerName
                    ),
                    embed_color::kGameOpen
                );
            }
        )
        .leak();

    GameEvent(GameEventType::Exiting)
        .listen(
            [] {
                auto& session = gameSession();
                if (!session.started) {
                    return;
                }
                if (!Mod::get()
                         ->getSettingValue<bool>("notify-game-session")) {
                    return;
                }
                auto const playerName = getPlayerName();
                auto const elapsed =
                    formatDuration(secondsSince(session.startTime));
                if (Mod::get()
                        ->getSettingValue<bool>("blocking-webhook")) {
                    sendWebhookDirectSync(
                        "Closed Geometry Dash",
                        fmt::format(
                            "{} exited Geometry Dash after {}.",
                            playerName,
                            elapsed
                        ),
                        embed_color::kGameClose,
                        {},
                        elapsed
                    );
                } else {
                    sendWebhookDirect(
                        "Closed Geometry Dash",
                        fmt::format(
                            "{} exited Geometry Dash after {}.",
                            playerName,
                            elapsed
                        ),
                        embed_color::kGameClose,
                        {},
                        elapsed
                    );
                }
            }
        )
        .leak();
}

$on_mod(Loaded)
{
    listenForSettingChanges<bool>("test-webhook", [](bool enabled) {
        if (!enabled) {
            return;
        }
        auto const playerName = getPlayerName();
        sendWebhookDirect(
            "Test Webhook",
            fmt::format(
                "{} is testing the webhook!",
                playerName
            ),
            embed_color::kTestWebhook
        );
        Mod::get()->setSettingValue<bool>("test-webhook", false);
    });
}
