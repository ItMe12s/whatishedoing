#pragma once

#include <Geode/Geode.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct WebhookField {
    std::string name;
    std::string value;
    bool inlineField = true;
};

std::string formatDuration(int totalSeconds);
std::string formatDurationMs(int64_t totalMs);

void sendWebhookDirect(
    std::string const& title,
    std::string const& description,
    int color,
    std::vector<WebhookField> const& fields = {},
    std::string const& footer = "",
    std::optional<std::vector<std::uint8_t>> screenshotPng = std::nullopt
);

void sendWebhookDirectSync(
    std::string const& title,
    std::string const& description,
    int color,
    std::vector<WebhookField> const& fields = {},
    std::string const& footer = "",
    std::optional<std::vector<std::uint8_t>> screenshotPng = std::nullopt
);

void sendWebhook(
    std::string const& settingKey,
    std::string const& title,
    std::string const& description,
    int color,
    std::vector<WebhookField> const& fields = {},
    std::string const& footer = "",
    std::optional<std::vector<std::uint8_t>> screenshotPng = std::nullopt
);

// Plain-message webhook (JSON content field only), all configured URLs and retries.
void sendWebhookContent(std::string const& content);
