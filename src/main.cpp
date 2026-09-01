#include "embed_colors.hpp"
#include "level_upload/custom_text.hpp"
#include "profile/popup.hpp"
#include "state.hpp"
#include "text_policy.hpp"
#include "webhook.hpp"

#include <Geode/Geode.hpp>
#include <Geode/loader/GameEvent.hpp>
#include <Geode/loader/SettingV3.hpp>

using namespace geode::prelude;

$execute {
    GameEvent(GameEventType::Loaded)
        .listen([] {
            auto& session = gameSession();
            if (session.started) {
                return;
            }
            session.started = true;
            session.timer.reset();
            auto const playerName = getPlayerName();
            sendWebhookIfEnabled(
                "notify-game-session",
                WebhookMessage{
                    .title = "Opened Geometry Dash",
                    .description = fmt::format("{} opened Geometry Dash!", playerName),
                    .color = embed_color::fromKey("color-game-open"),
                }
            );
        })
        .leak();

    GameEvent(GameEventType::Exiting)
        .listen([] {
            auto& session = gameSession();
            if (!session.started) {
                return;
            }
            if (!Mod::get()->getSettingValue<bool>("notify-game-session")) {
                return;
            }
            auto const playerName = getPlayerName();
            auto const elapsed = text_policy::formatDuration(
                static_cast<int>(session.timer.elapsed<std::chrono::seconds>())
            );
            WebhookMessage message{
                .title = "Closed Geometry Dash",
                .description = fmt::format("{} closed Geometry Dash.", playerName),
                .color = embed_color::fromKey("color-game-close"),
                .footer = elapsed,
            };
            if (Mod::get()->getSettingValue<bool>("blocking-webhook")) {
                sendWebhookBlocking(std::move(message));
            }
            else {
                sendWebhook(std::move(message));
            }
        })
        .leak();
}

$on_mod(Loaded) {
    ButtonSettingPressedEventV3(Mod::get(), "profile-manager")
        .listen([](std::string_view buttonKey) {
            if (buttonKey == "manage") {
                profile::ProfileManagerPopup::create()->show();
            }
        })
        .leak();

    ButtonSettingPressedEventV3(Mod::get(), "upload-open-custom-text")
        .listen([](std::string_view buttonKey) {
            if (buttonKey != "edit") {
                return;
            }
            if (!Mod::get()->getSettingValue<bool>("upload-use-custom-text")) {
                return;
            }
            level_upload::revealCustomTextFileFromSettings();
        })
        .leak();

    listenForSettingChanges<bool>("test-webhook", [](bool enabled) {
        if (!enabled) {
            return;
        }
        auto const playerName = getPlayerName();
        sendWebhook(
            WebhookMessage{
                .title = "Test Webhook",
                .description = fmt::format("{} is testing the webhook!", playerName),
                .color = embed_color::fromKey("color-test-webhook"),
            }
        );
        Mod::get()->setSettingValue<bool>("test-webhook", false);
    });
}
