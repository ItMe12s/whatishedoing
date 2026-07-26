#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct WebhookField {
    std::string name;
    std::string value;
    bool inlineField = true;
};

struct WebhookMessage {
    std::string title;
    std::string description;
    int color = 0;
    std::vector<WebhookField> fields;
    std::string footer;
    std::optional<std::vector<std::uint8_t>> screenshotPng;
};

void sendWebhook(WebhookMessage message);
void sendWebhookIfEnabled(std::string const& settingKey, WebhookMessage message);
void sendWebhookBlocking(WebhookMessage message);

void sendWebhookContent(std::string const& content);
