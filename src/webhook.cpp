#include "webhook.hpp"

#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>
#include <chrono>
#include <ctime>
#include <optional>
#include <thread>

using namespace geode::prelude;

namespace {

std::string currentIso8601Utc() {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &tt);
#else
    gmtime_r(&tt, &utc);
#endif
    char buf[21];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return buf;
}

matjson::Value buildWebhookPayload(
    std::string const& title,
    std::string const& description,
    int color,
    std::vector<WebhookField> const& fields,
    std::string const& footer
) {
    auto fieldsArr = matjson::Value::array();
    for (auto const& f : fields) {
        auto obj = matjson::Value::object();
        obj["name"] = f.name;
        obj["value"] = f.value;
        obj["inline"] = f.inline_field;
        fieldsArr.push(obj);
    }

    auto embed = matjson::Value::object();
    embed["title"]       = title;
    embed["description"] = description;
    embed["color"]       = color;
    embed["fields"]      = fieldsArr;
    embed["timestamp"]   = currentIso8601Utc();
    if (!footer.empty()) {
        auto footerObj = matjson::Value::object();
        footerObj["text"] = footer;
        embed["footer"] = footerObj;
    }

    auto embedsArr = matjson::Value::array();
    embedsArr.push(embed);

    auto payload = matjson::Value::object();
    payload["username"] = Mod::get()->getSettingValue<std::string>("webhook-username");
    payload["embeds"]   = embedsArr;
    return payload;
}

std::optional<int> backoffSecondsForFailedAttempt(
    web::WebResponse const& res,
    int attempt,
    int maxRetries
) {
    if (attempt >= maxRetries) {
        log::warn(
            "Webhook POST failed after {} attempts (status {})",
            attempt + 1,
            res.code()
        );
        return std::nullopt;
    }

    auto waitSeconds = 1 << attempt;
    log::warn(
        "Webhook POST failed (status {}), retrying in {}s ({}/{})",
        res.code(),
        waitSeconds,
        attempt + 1,
        maxRetries + 1
    );
    return waitSeconds;
}

void postWebhookSyncWithRetries(
    std::string const& url,
    matjson::Value const& payload,
    int attempt,
    int maxRetries
) {
    auto req = web::WebRequest();
    req.header("Content-Type", "application/json");
    req.bodyJSON(payload);
    req.timeout(std::chrono::seconds(10));

    auto res = req.postSync(url);
    if (res.ok()) return;

    auto wait = backoffSecondsForFailedAttempt(res, attempt, maxRetries);
    if (!wait) return;

    std::this_thread::sleep_for(std::chrono::seconds(*wait));
    postWebhookSyncWithRetries(url, payload, attempt + 1, maxRetries);
}

void postWebhookWithRetries(
    std::string const& url,
    matjson::Value payload,
    int attempt,
    int maxRetries
) {
    auto req = web::WebRequest();
    req.header("Content-Type", "application/json");
    req.bodyJSON(payload);
    req.timeout(std::chrono::seconds(10));

    async::spawn(
        req.post(url),
        [url, payload = std::move(payload), attempt, maxRetries](web::WebResponse res) mutable {
            if (res.ok()) return;

            auto wait = backoffSecondsForFailedAttempt(res, attempt, maxRetries);
            if (!wait) return;

            std::this_thread::sleep_for(std::chrono::seconds(*wait));
            postWebhookWithRetries(url, std::move(payload), attempt + 1, maxRetries);
        }
    );
}

} // namespace

std::string formatDuration(int totalSeconds) {
    auto h = totalSeconds / 3600;
    auto m = (totalSeconds % 3600) / 60;
    auto s = totalSeconds % 60;

    auto unit = [](int n, const char* u) {
        return fmt::format("{} {}{}", n, u, n == 1 ? "" : "s");
    };

    std::vector<std::string> parts;
    if (h) parts.push_back(unit(h, "hour"));
    if (m) parts.push_back(unit(m, "minute"));
    if (s || parts.empty()) parts.push_back(unit(s, "second"));

    if (parts.size() == 1) return parts[0];
    if (parts.size() == 2) return parts[0] + " and " + parts[1];
    return parts[0] + ", " + parts[1] + " and " + parts[2];
}

void sendWebhookDirect(
    std::string const& title,
    std::string const& description,
    int color,
    std::vector<WebhookField> const& fields,
    std::string const& footer
) {
    auto url = Mod::get()->getSettingValue<std::string>("webhook-url");
    if (url.empty()) return;

    auto payload = buildWebhookPayload(title, description, color, fields, footer);
    auto maxRetries = static_cast<int>(Mod::get()->getSettingValue<int64_t>("max-retries"));
    if (maxRetries < 0) maxRetries = 0;

    if (Mod::get()->getSettingValue<bool>("blocking-webhook")) {
        postWebhookSyncWithRetries(url, payload, 0, maxRetries);
    } else {
        postWebhookWithRetries(url, std::move(payload), 0, maxRetries);
    }
}

void sendWebhook(
    std::string const& settingKey,
    std::string const& title,
    std::string const& description,
    int color,
    std::vector<WebhookField> const& fields,
    std::string const& footer
) {
    if (!Mod::get()->getSettingValue<bool>(settingKey)) return;
    sendWebhookDirect(title, description, color, fields, footer);
}
