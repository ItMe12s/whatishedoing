#include "webhook.hpp"

#include <Geode/utils/async.hpp>
#include <Geode/utils/general.hpp>
#include <Geode/utils/string.hpp>
#include <Geode/utils/web.hpp>
#include <matjson.hpp>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>
#include <vector>

using namespace geode::prelude;

namespace webhook_impl {

constexpr auto kAsyncRequestTimeout = std::chrono::seconds(10);
constexpr auto kSyncRequestTimeout = std::chrono::seconds(3);
constexpr int kSyncMaxRetries = 1;
constexpr int kSyncMaxRetryDelaySeconds = 2;

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

std::vector<std::string> collectWebhookTargets() {
    static constexpr char const* kExtraKeys[] = {
        "extra-webhook-url-1",
        "extra-webhook-url-2",
        "extra-webhook-url-3",
        "extra-webhook-url-4",
    };
    std::vector<std::string> out;
    if (auto u = normalizeWebhookUrl(
            Mod::get()->getSettingValue<std::string>("webhook-url"))) {
        out.push_back(std::move(*u));
    }
    for (auto* key : kExtraKeys) {
        if (auto u = normalizeWebhookUrl(
                Mod::get()->getSettingValue<std::string>(key))) {
            out.push_back(std::move(*u));
        }
    }
    return out;
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
    std::string const& footer,
    bool embedScreenshotAttachment
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
    if (embedScreenshotAttachment) {
        auto imgObj = matjson::Value::object();
        imgObj["url"] = "attachment://screenshot.png";
        embed["image"] = imgObj;
    }

    auto embedsArr = matjson::Value::array();
    embedsArr.push(embed);

    auto payload = matjson::Value::object();
    auto username = geode::utils::string::trim(
        Mod::get()->getSettingValue<std::string>("webhook-username"));
    if (!username.empty()) {
        payload["username"] = std::move(username);
    }
    payload["embeds"] = embedsArr;
    return payload;
}

std::optional<int> backoffSecondsForFailedAttempt(
    web::WebResponse const& res,
    int attempt,
    int maxRetries,
    std::optional<int> maxDelaySeconds = std::nullopt
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
            return n.mapOr(2, [maxDelaySeconds](int v) {
                int const maxDelay = maxDelaySeconds.value_or(86'400);
                return std::clamp(v, 1, maxDelay);
            });
        }
        return maxDelaySeconds
            ? std::min(2, *maxDelaySeconds)
            : 2;
    }
    int const wait = 1 << attempt;
    int const clampedWait = maxDelaySeconds
        ? std::min(wait, *maxDelaySeconds)
        : wait;
    log::warn(
        "Webhook POST failed (status {}), retrying in {}s ({}/{})",
        res.code(),
        clampedWait,
        attempt + 1,
        maxRetries + 1
    );
    return clampedWait;
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
    req.timeout(kSyncRequestTimeout);

    auto res = req.postSync(url);
    if (res.ok()) return;

    auto wait = backoffSecondsForFailedAttempt(
        res,
        attempt,
        maxRetries,
        kSyncMaxRetryDelaySeconds
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
    req.timeout(kAsyncRequestTimeout);

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
            async::runtime().spawnBlocking<void>(
                [url, payload = std::move(payload), delaySec, attempt, maxRetries]() mutable {
                    std::this_thread::sleep_for(std::chrono::seconds(delaySec));
                    geode::queueInMainThread(
                        [url = std::move(url),
                         payload = std::move(payload),
                         attempt,
                         maxRetries]() mutable {
                            postWebhookWithRetries(
                                url,
                                std::move(payload),
                                attempt + 1,
                                maxRetries
                            );
                        }
                    );
                }
            );
        }
    );
}

void postWebhookMultipartWithRetries(
    std::string const& url,
    std::string const& payloadJson,
    std::shared_ptr<std::vector<std::uint8_t> const> pngBytes,
    int attempt,
    int maxRetries
) {
    if (!pngBytes) {
        return;
    }
    web::MultipartForm form;
    form.param("payload_json", payloadJson);
    form.file("files[0]", *pngBytes, "screenshot.png", "image/png");
    auto req = web::WebRequest();
    req.bodyMultipart(std::move(form));
    req.timeout(kAsyncRequestTimeout);

    async::spawn(
        req.post(url),
        [url, payloadJson, pngBytes = std::move(pngBytes), attempt, maxRetries](
            web::WebResponse res
        ) mutable {
            if (res.ok()) return;
            auto wait = backoffSecondsForFailedAttempt(
                res,
                attempt,
                maxRetries
            );
            if (!wait) return;
            int const delaySec = *wait;
            async::runtime().spawnBlocking<void>(
                [url, payloadJson, pngBytes = std::move(pngBytes), delaySec, attempt, maxRetries]() mutable {
                    std::this_thread::sleep_for(std::chrono::seconds(delaySec));
                    geode::queueInMainThread(
                        [url = std::move(url),
                         payloadJson = std::move(payloadJson),
                         pngBytes = std::move(pngBytes),
                         attempt,
                         maxRetries]() mutable {
                            postWebhookMultipartWithRetries(
                                url,
                                payloadJson,
                                std::move(pngBytes),
                                attempt + 1,
                                maxRetries
                            );
                        }
                    );
                }
            );
        }
    );
}

void postWebhookSyncMultipartWithRetries(
    std::string const& url,
    std::string const& payloadJson,
    std::vector<std::uint8_t> const& pngBytes,
    int attempt,
    int maxRetries
) {
    web::MultipartForm form;
    form.param("payload_json", payloadJson);
    form.file("files[0]", pngBytes, "screenshot.png", "image/png");
    auto req = web::WebRequest();
    req.bodyMultipart(std::move(form));
    req.timeout(kSyncRequestTimeout);

    auto res = req.postSync(url);
    if (res.ok()) return;

    auto wait = backoffSecondsForFailedAttempt(
        res,
        attempt,
        maxRetries,
        kSyncMaxRetryDelaySeconds
    );
    if (!wait) return;

    std::this_thread::sleep_for(
        std::chrono::seconds(*wait)
    );
    postWebhookSyncMultipartWithRetries(
        url,
        payloadJson,
        pngBytes,
        attempt + 1,
        maxRetries
    );
}

void sendImpl(
    bool useSync,
    std::string const& title,
    std::string const& description,
    int color,
    std::vector<WebhookField> const& fields,
    std::string const& footer,
    std::optional<std::vector<std::uint8_t>> screenshotPng
) {
    auto urls = collectWebhookTargets();
    if (urls.empty()) {
        return;
    }
    auto maxRetries = static_cast<int>(
        Mod::get()->getSettingValue<int64_t>("max-retries")
    );
    if (maxRetries < 0) {
        maxRetries = 0;
    }
    int const effectiveMaxRetries = useSync
        ? std::min(maxRetries, kSyncMaxRetries)
        : maxRetries;

    bool const hasShot =
        screenshotPng.has_value() && !screenshotPng->empty();

    if (hasShot) {
        auto payload = buildWebhookPayload(
            title,
            description,
            color,
            fields,
            footer,
            true
        );
        auto payloadJson = payload.dump(matjson::NO_INDENTATION);
        if (useSync) {
            std::vector<std::uint8_t> const& bytes = *screenshotPng;
            for (auto const& url : urls) {
                postWebhookSyncMultipartWithRetries(
                    url,
                    payloadJson,
                    bytes,
                    0,
                    effectiveMaxRetries
                );
            }
        } else {
            auto sharedBytes =
                std::make_shared<std::vector<std::uint8_t> const>(
                    std::move(*screenshotPng)
                );
            for (auto const& url : urls) {
                postWebhookMultipartWithRetries(
                    url,
                    payloadJson,
                    sharedBytes,
                    0,
                    effectiveMaxRetries
                );
            }
        }
        return;
    }

    auto payload = buildWebhookPayload(
        title,
        description,
        color,
        fields,
        footer,
        false
    );
    if (useSync) {
        for (auto const& url : urls) {
            postWebhookSyncWithRetries(
                url,
                matjson::Value(payload),
                0,
                effectiveMaxRetries
            );
        }
    } else {
        for (auto const& url : urls) {
            postWebhookWithRetries(
                url,
                matjson::Value(payload),
                0,
                effectiveMaxRetries
            );
        }
    }
}

matjson::Value buildContentWebhookPayload(std::string const& content) {
    auto payload = matjson::Value::object();
    auto username = geode::utils::string::trim(
        Mod::get()->getSettingValue<std::string>("webhook-username"));
    if (!username.empty()) {
        payload["username"] = std::move(username);
    }
    payload["content"] = content;
    return payload;
}

void sendContentImpl(std::string const& content) {
    auto urls = collectWebhookTargets();
    if (urls.empty()) {
        return;
    }
    auto maxRetries = static_cast<int>(
        Mod::get()->getSettingValue<int64_t>("max-retries")
    );
    if (maxRetries < 0) {
        maxRetries = 0;
    }
    auto base = buildContentWebhookPayload(content);
    for (auto const& url : urls) {
        postWebhookWithRetries(
            url,
            matjson::Value(base),
            0,
            maxRetries
        );
    }
}

} // namespace webhook_impl

void sendWebhookContent(std::string const& content) {
    std::string body = content;
    constexpr size_t kMaxDiscordContent = 2000;
    if (body.size() > kMaxDiscordContent) {
        log::warn(
            "Webhook message content truncated from {} to {} characters",
            body.size(),
            kMaxDiscordContent
        );
        body.resize(kMaxDiscordContent);
    }
    webhook_impl::sendContentImpl(body);
}

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

void sendWebhookDirect(
    std::string const& title,
    std::string const& description,
    int color,
    std::vector<WebhookField> const& fields,
    std::string const& footer,
    std::optional<std::vector<std::uint8_t>> screenshotPng
) {
    webhook_impl::sendImpl(
        false,
        title,
        description,
        color,
        fields,
        footer,
        std::move(screenshotPng)
    );
}

void sendWebhookDirectSync(
    std::string const& title,
    std::string const& description,
    int color,
    std::vector<WebhookField> const& fields,
    std::string const& footer,
    std::optional<std::vector<std::uint8_t>> screenshotPng
) {
    webhook_impl::sendImpl(
        true,
        title,
        description,
        color,
        fields,
        footer,
        std::move(screenshotPng)
    );
}

void sendWebhook(
    std::string const& settingKey,
    std::string const& title,
    std::string const& description,
    int color,
    std::vector<WebhookField> const& fields,
    std::string const& footer,
    std::optional<std::vector<std::uint8_t>> screenshotPng
) {
    if (!Mod::get()->getSettingValue<bool>(settingKey)) {
        return;
    }
    sendWebhookDirect(
        title,
        description,
        color,
        fields,
        footer,
        std::move(screenshotPng)
    );
}
