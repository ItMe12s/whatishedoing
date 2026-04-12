#pragma once

#include <Geode/Geode.hpp>

using namespace geode::prelude;

struct WebhookField {
    std::string name;
    std::string value;
    bool inline_field = true;
};

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
