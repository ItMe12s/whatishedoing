#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct WebhookMessage {
    std::string title;
    std::string description;
    int color = 0;
    std::string footer;
    std::optional<std::string> difficultyFace;
    std::optional<std::vector<std::uint8_t>> screenshotPng;
};

void sendWebhook(WebhookMessage message);
void sendWebhookIfEnabled(std::string const& settingKey, WebhookMessage message);
void sendWebhookBlocking(WebhookMessage message);

void sendWebhookContent(std::string const& content);
