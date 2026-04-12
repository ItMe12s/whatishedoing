#include "webhook.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

void sendWebhook(
    std::string const& title,
    std::string const& description,
    int                color,
    std::vector<WebhookField> const& fields
) {
    auto url = Mod::get()->getSettingValue<std::string>("webhook-url");
    if (url.empty()) return;

    auto username = Mod::get()->getSettingValue<std::string>("webhook-username");

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

    auto payload = matjson::Value::object();
    payload["username"] = username;
    payload["embeds"]   = embedsArr;

    auto req = web::WebRequest();
    req.header("Content-Type", "application/json");
    req.bodyJSON(payload);

    static TaskHolder<web::WebResponse> s_task;
    s_task.spawn(
        req.post(url),
        [](web::WebResponse res) {
            if (!res.ok()) {
                log::warn("Webhook POST failed with status {}", res.code());
            }
        }
    );
}
