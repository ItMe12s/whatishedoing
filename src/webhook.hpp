#pragma once

#include <string>
#include <vector>

struct WebhookField {
    std::string name;
    std::string value;
    bool        inline_field = true;
};

void sendWebhook(
    std::string const& title,
    std::string const& description,
    int                color,
    std::vector<WebhookField> const& fields = {}
);
