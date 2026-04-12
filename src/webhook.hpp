#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>
#include <chrono>
#include <ctime>
#include <thread>

using namespace geode::prelude;

struct WebhookField {
    std::string name;
    std::string value;
    bool inline_field = true;
};

inline std::string currentIso8601Utc() {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm utc {};
#ifdef _WIN32
    gmtime_s(&utc, &tt);
#else
    gmtime_r(&tt, &utc);
#endif
    return fmt::format(
        "{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}Z",
        utc.tm_year + 1900,
        utc.tm_mon + 1,
        utc.tm_mday,
        utc.tm_hour,
        utc.tm_min,
        utc.tm_sec
    );
}

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
    std::vector<WebhookField> const& fields = {},
    std::string const& footer = ""
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
    if (!footer.empty()) {
        auto footerObj = matjson::Value::object();
        footerObj["text"] = footer;
        embed["footer"] = footerObj;
    }
    embed["timestamp"] = currentIso8601Utc();

    auto embedsArr = matjson::Value::array();
    embedsArr.push(embed);

    auto username = Mod::get()->getSettingValue<std::string>("webhook-username");

    auto payload = matjson::Value::object();
    payload["username"] = username;
    payload["embeds"]   = embedsArr;

    return payload;
}

inline void postWebhookWithRetries(
    std::string const& url,
    matjson::Value payload,
    int attempt,
    int maxRetries
) {
    auto req = web::WebRequest();
    req.header("Content-Type", "application/json");
    req.bodyJSON(payload);

    async::spawn(
        req.post(url),
        [url, payload, attempt, maxRetries](web::WebResponse res) {
            if (res.ok()) return;

            if (attempt >= maxRetries) {
                log::warn(
                    "Webhook POST failed after {} attempts (status {})",
                    attempt + 1,
                    res.code()
                );
                return;
            }

            auto waitSeconds = 1 << attempt;
            log::warn(
                "Webhook POST failed (status {}), retrying in {}s ({}/{})",
                res.code(),
                waitSeconds,
                attempt + 1,
                maxRetries + 1
            );

            std::thread([url, payload, attempt, maxRetries, waitSeconds]() {
                std::this_thread::sleep_for(std::chrono::seconds(waitSeconds));
                postWebhookWithRetries(url, payload, attempt + 1, maxRetries);
            }).detach();
        }
    );
}

inline void sendWebhookDirect(
    std::string const& title,
    std::string const& description,
    int color,
    std::vector<WebhookField> const& fields = {},
    std::string const& footer = ""
) {
    auto url = Mod::get()->getSettingValue<std::string>("webhook-url");
    if (url.empty()) return;

    auto payload = buildWebhookPayload(title, description, color, fields, footer);
    auto maxRetries = static_cast<int>(Mod::get()->getSettingValue<int64_t>("max-retries"));
    if (maxRetries < 0) maxRetries = 0;
    postWebhookWithRetries(url, payload, 0, maxRetries);
}

inline void sendWebhook(
    std::string const& settingKey,
    std::string const& title,
    std::string const& description,
    int color,
    std::vector<WebhookField> const& fields = {},
    std::string const& footer = ""
) {
    if (!Mod::get()->getSettingValue<bool>(settingKey)) return;
    sendWebhookDirect(title, description, color, fields, footer);
}
