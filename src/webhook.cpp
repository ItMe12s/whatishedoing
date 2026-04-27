#include "webhook.hpp"

#include <Geode/utils/async.hpp>
#include <Geode/utils/general.hpp>
#include <Geode/utils/string.hpp>
#include <Geode/utils/web.hpp>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <optional>
#include <string_view>
#include <thread>

using namespace geode::prelude;

namespace {

// Returns nullopt if the URL is missing or not suitable for a Discord
// webhook POST.
std::optional<std::string> normalizeWebhookUrl(std::string const& raw) {
    std::string url = raw;
    geode::utils::string::trimIP(url);
    if (url.empty()) return std::nullopt;
    if (url.rfind("https://", 0) != 0) {
        log::warn("Webhook URL must start with https://");
        return std::nullopt;
    }
    return url;
}

std::string currentIso8601Utc() {
    auto const now = std::chrono::system_clock::now();
    auto const tt = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &tt);
#else
    gmtime_r(&tt, &utc);
#endif
    char buf[32];
    std::strftime(
        buf,
        sizeof(buf),
        "%Y-%m-%dT%H:%M:%SZ",
        &utc
    );
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
        obj["inline"] = f.inlineField;
        fieldsArr.push(obj);
    }

    auto embed = matjson::Value::object();
    embed["title"] = title;
    embed["description"] = description;
    embed["color"] = color;
    embed["fields"] = fieldsArr;
    embed["timestamp"] = currentIso8601Utc();
    if (!footer.empty()) {
        auto footerObj = matjson::Value::object();
        footerObj["text"] = footer;
        embed["footer"] = footerObj;
    }

    auto embedsArr = matjson::Value::array();
    embedsArr.push(embed);

    auto payload = matjson::Value::object();
    payload["username"] =
        Mod::get()->getSettingValue<std::string>("webhook-username");
    payload["embeds"] = embedsArr;
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
    if (res.code() == 429) {
        auto const ra = res.header("Retry-After");
        if (ra && !ra->empty()) {
            auto const n = geode::utils::numFromString<int>(
                std::string_view{*ra},
                10
            );
            return n.mapOr(2, [](int v) {
                return std::clamp(v, 1, 86'400);
            });
        }
        return 2;
    }
    int const wait = 1 << attempt;
    log::warn(
        "Webhook POST failed (status {}), retrying in {}s ({}/{})",
        res.code(),
        wait,
        attempt + 1,
        maxRetries + 1
    );
    return wait;
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

    auto wait = backoffSecondsForFailedAttempt(
        res,
        attempt,
        maxRetries
    );
    if (!wait) return;

    std::this_thread::sleep_for(
        std::chrono::seconds(*wait)
    );
    postWebhookSyncWithRetries(
        url,
        payload,
        attempt + 1,
        maxRetries
    );
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
        [url, payload = std::move(payload), attempt, maxRetries](
            web::WebResponse res) mutable {
            if (res.ok()) return;
            auto wait = backoffSecondsForFailedAttempt(
                res,
                attempt,
                maxRetries
            );
            if (!wait) return;
            int const delaySec = *wait;
            std::thread(
                [url, payload = std::move(payload), delaySec, attempt, maxRetries]() mutable {
                    std::this_thread::sleep_for(
                        std::chrono::seconds(delaySec)
                    );
                    postWebhookWithRetries(
                        url,
                        std::move(payload),
                        attempt + 1,
                        maxRetries
                    );
                }
            )
                .detach();
        }
    );
}

} // namespace

std::string formatDuration(int totalSeconds) {
    auto h = totalSeconds / 3600;
    auto m = (totalSeconds % 3600) / 60;
    auto s = totalSeconds % 60;

    auto unit = [](int n, char const* u) {
        return fmt::format(
            "{} {}{}",
            n,
            u,
            n == 1 ? "" : "s"
        );
    };

    std::vector<std::string> parts;
    if (h) parts.push_back(unit(h, "hour"));
    if (m) parts.push_back(unit(m, "minute"));
    if (s || parts.empty()) {
        parts.push_back(unit(s, "second"));
    }
    if (parts.size() == 1) return parts[0];
    if (parts.size() == 2) {
        return parts[0] + " and " + parts[1];
    }
    return parts[0] + ", " + parts[1] + " and " + parts[2];
}

std::string formatDurationMs(int64_t totalMs) {
    if (totalMs < 0) {
        totalMs = 0;
    }
    if (totalMs < 1000) {
        if (totalMs == 0) {
            return "0 seconds";
        }
        return fmt::format(
            "{:.2f} seconds",
            static_cast<double>(totalMs) / 1000.0
        );
    }
    return formatDuration(
        static_cast<int>(totalMs / 1000)
    );
}

void sendImpl(
    bool useSync,
    std::string const& title,
    std::string const& description,
    int color,
    std::vector<WebhookField> const& fields,
    std::string const& footer
) {
    auto const urlOpt =
        normalizeWebhookUrl(
            Mod::get()->getSettingValue<std::string>("webhook-url")
        );
    if (!urlOpt) return;
    std::string const& url = *urlOpt;
    auto payload = buildWebhookPayload(
        title,
        description,
        color,
        fields,
        footer
    );
    auto maxRetries = static_cast<int>(
        Mod::get()->getSettingValue<int64_t>("max-retries")
    );
    if (maxRetries < 0) {
        maxRetries = 0;
    }
    if (useSync) {
        postWebhookSyncWithRetries(url, payload, 0, maxRetries);
    } else {
        postWebhookWithRetries(
            url,
            std::move(payload),
            0,
            maxRetries
        );
    }
}

void sendWebhookDirect(
    std::string const& title,
    std::string const& description,
    int color,
    std::vector<WebhookField> const& fields,
    std::string const& footer
) {
    sendImpl(
        false,
        title,
        description,
        color,
        fields,
        footer
    );
}

void sendWebhookDirectSync(
    std::string const& title,
    std::string const& description,
    int color,
    std::vector<WebhookField> const& fields,
    std::string const& footer
) {
    sendImpl(
        true,
        title,
        description,
        color,
        fields,
        footer
    );
}

void sendWebhook(
    std::string const& settingKey,
    std::string const& title,
    std::string const& description,
    int color,
    std::vector<WebhookField> const& fields,
    std::string const& footer
) {
    if (!Mod::get()->getSettingValue<bool>(settingKey)) {
        return;
    }
    sendWebhookDirect(
        title,
        description,
        color,
        fields,
        footer
    );
}
