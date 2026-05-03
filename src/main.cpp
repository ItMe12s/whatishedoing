#include <Geode/Geode.hpp>
#include <Geode/loader/GameEvent.hpp>
#include <Geode/loader/SettingV3.hpp>

#include "embed_colors.hpp"
#include "level_upload/open_file_setting.hpp"
#include "profile/popup.hpp"
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
                session.eventCount = 0;
                if (session.eventCount < session.trackedEvents.size()) {
                    session.trackedEvents[session.eventCount++] = Tracked::GameOpen;
                }
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
                if (session.eventCount < session.trackedEvents.size()) {
                    session.trackedEvents[session.eventCount++] = Tracked::GameClose;
                }
                auto const playerName = getPlayerName();
                auto const elapsed =
                    formatDuration(secondsSince(session.startTime));
                std::string footer = elapsed;
                auto const summary = formatSessionTrackedSummary(session);
                if (!summary.empty()) {
                    footer += "\nSession: " + summary;
                    if (footer.size() > 2048) {
                        footer.resize(2045);
                        footer += "...";
                    }
                }
                if (Mod::get()
                        ->getSettingValue<bool>("blocking-webhook")) {
                    sendWebhookDirectSync(
                        "Closed Geometry Dash",
                        fmt::format(
                            "{} closed Geometry Dash.",
                            playerName
                        ),
                        embed_color::kGameClose,
                        {},
                        footer
                    );
                } else {
                    sendWebhookDirect(
                        "Closed Geometry Dash",
                        fmt::format(
                            "{} closed Geometry Dash.",
                            playerName
                        ),
                        embed_color::kGameClose,
                        {},
                        footer
                    );
                }
            }
        )
        .leak();
}

$on_mod(Loaded)
{
    ButtonSettingPressedEventV3(Mod::get(), "profile-manager")
        .listen(
            [](std::string_view buttonKey) {
                if (buttonKey == "manage") {
                    profile::ProfileManagerPopup::create()->show();
                }
            }
        )
        .leak();

    ButtonSettingPressedEventV3(Mod::get(), "upload-open-custom-text")
        .listen(
            [](std::string_view buttonKey) {
                if (buttonKey != "edit") {
                    return;
                }
                if (!Mod::get()->getSettingValue<bool>("upload-use-custom-text")) {
                    return;
                }
                level_upload::openCustomTextFileFromSettings();
            }
        )
        .leak();

    listenForSettingChanges<bool>("test-webhook", [](bool enabled) {
        if (!enabled) {
            return;
        }
        auto const playerName = getPlayerName();
        auto& session = gameSession();
        if (session.eventCount < session.trackedEvents.size()) {
            session.trackedEvents[session.eventCount++] = Tracked::TestWebhook;
        }
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
