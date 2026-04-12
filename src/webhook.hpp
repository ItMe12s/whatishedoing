#pragma once

#include <Geode/Geode.hpp>
#include <string>
#include <vector>

struct WebhookField {
    std::string name;
    std::string value;
    bool inline_field = true;
};

std::string formatDuration(int totalSeconds);

void sendWebhookDirect(
    std::string const& title,
    std::string const& description,
    int color,
    std::vector<WebhookField> const& fields = {},
    std::string const& footer = ""
);

void sendWebhook(
    std::string const& settingKey,
    std::string const& title,
    std::string const& description,
    int color,
    std::vector<WebhookField> const& fields = {},
    std::string const& footer = ""
);
