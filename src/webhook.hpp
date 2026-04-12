#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

struct WebhookField {
    std::string name;
    std::string value;
    bool inline_field = true;
};

inline std::string formatDuration(int totalSeconds) {
    if (totalSeconds < 0) totalSeconds = 0;

    auto hours = totalSeconds / 3600;
    auto minutes = (totalSeconds % 3600) / 60;
    auto seconds = totalSeconds % 60;

    std::vector<std::string> parts;
    if (hours > 0) {
        parts.push_back(fmt::format("{} hour{}", hours, hours == 1 ? "" : "s"));
    }
    if (minutes > 0) {
        parts.push_back(fmt::format("{} minute{}", minutes, minutes == 1 ? "" : "s"));
    }
    if (seconds > 0 || parts.empty()) {
        parts.push_back(fmt::format("{} second{}", seconds, seconds == 1 ? "" : "s"));
    }

    if (parts.size() == 1) return parts[0];
    if (parts.size() == 2) return fmt::format("{} and {}", parts[0], parts[1]);
    return fmt::format("{}, {} and {}", parts[0], parts[1], parts[2]);
}

inline matjson::Value buildWebhookPayload(
    std::string const& title,
    std::string const& description,
    int color,
    std::vector<WebhookField> const& fields = {}
) {
    auto fieldsArr = matjson::Value::array();
    for (auto const& f : fields) {
        auto obj = matjson::Value::object();
        obj["name"]   = f.name;
        obj["value"]  = f.value;
        obj["inline"] = f.inline_field;
        fieldsArr.push(obj);
    }

    auto embed = matjson::Value::object();
    embed["title"]       = title;
    embed["description"] = description;
    embed["color"]       = color;
    embed["fields"]      = fieldsArr;

    auto embedsArr = matjson::Value::array();
    embedsArr.push(embed);

    auto username = Mod::get()->getSettingValue<std::string>("webhook-username");

    auto payload = matjson::Value::object();
    payload["username"] = username;
    payload["embeds"]   = embedsArr;

    return payload;
}

inline void sendWebhook(
    std::string const& settingKey,
    std::string const& title,
    std::string const& description,
    int color,
    std::vector<WebhookField> const& fields = {}
) {
    if (!Mod::get()->getSettingValue<bool>(settingKey)) return;

    auto url = Mod::get()->getSettingValue<std::string>("webhook-url");
    if (url.empty()) return;

    auto payload = buildWebhookPayload(title, description, color, fields);

    auto req = web::WebRequest();
    req.header("Content-Type", "application/json");
    req.bodyJSON(payload);

    async::spawn(
        req.post(url),
        [](web::WebResponse res) {
            if (!res.ok()) {
                log::warn("Webhook POST failed with status {}", res.code());
            }
        }
    );
}
