#include "webhook.hpp"

#include "retry_policy.hpp"
#include "text_policy.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/ranges.hpp>
#include <Geode/utils/string.hpp>
#include <Geode/utils/web.hpp>
#include <algorithm>
#include <arc/time/Sleep.hpp>
#include <array>
#include <asp/time/SystemTime.hpp>
#include <chrono>
#include <cstdint>
#include <matjson.hpp>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using namespace geode::prelude;

namespace webhook_impl {

    constexpr auto kAsyncRequestTimeout = std::chrono::seconds(10);
    constexpr auto kSyncRequestTimeout = std::chrono::seconds(3);
    constexpr int kSyncMaxRetries = 1;
    constexpr int kSyncMaxRetryDelaySeconds = 2;

    constexpr size_t kDiscordEmbedTitleMax = 256;
    constexpr size_t kDiscordEmbedDescriptionMax = 4096;
    constexpr size_t kDiscordEmbedFieldNameMax = 256;
    constexpr size_t kDiscordEmbedFieldValueMax = 1024;
    constexpr size_t kDiscordEmbedFooterMax = 2048;
    constexpr size_t kDiscordWebhookUsernameMax = 80;
    constexpr size_t kDiscordEmbedFieldCountMax = 25;

    static std::string clampUtf8ByBytes(std::string s, size_t maxBytes, char const* ctx) {
        if (s.size() <= maxBytes) {
            return s;
        }
        log::warn("{} truncated from {} to {} bytes", ctx, s.size(), maxBytes);
        return text_policy::clampUtf8ByBytes(s, maxBytes);
    }

    static bool isAllowedDiscordWebhookHost(std::string const& hostLower) {
        static constexpr std::array<std::string_view, 4> kAllowedHosts = {
            "discord.com",
            "discordapp.com",
            "canary.discord.com",
            "ptb.discord.com",
        };
        return geode::utils::ranges::contains(kAllowedHosts, std::string_view(hostLower));
    }

    static std::string_view safeWebErrorMessage(web::WebResponse const& res) {
        auto const error = res.errorMessage();
        if (error.empty() || geode::utils::string::contains(error, "://") ||
            geode::utils::string::contains(error, "/api/webhooks/") ||
            error.find_first_of("\r\n") != std::string_view::npos) {
            return {};
        }
        return error;
    }

    // Returns nullopt if the URL is missing or not suitable for a Discord webhook POST.
    std::optional<std::string> normalizeWebhookUrl(std::string const& raw) {
        std::string url = raw;
        geode::utils::string::trimIP(url);
        if (url.empty()) return std::nullopt;
        if (!geode::utils::string::startsWith(url, "https://")) {
            log::warn("Webhook URL must start with https://");
            return std::nullopt;
        }
        if (!geode::utils::string::contains(url, "/api/webhooks/")) {
            log::warn("Webhook URL must include Discord path /api/webhooks/");
            return std::nullopt;
        }
        size_t const hostStart = 8;
        size_t pathStart = url.find('/', hostStart);
        if (pathStart == std::string::npos) {
            log::warn("Webhook URL missing path after host");
            return std::nullopt;
        }
        std::string host = url.substr(hostStart, pathStart - hostStart);
        if (auto const at = host.rfind('@'); at != std::string::npos) {
            host = host.substr(at + 1);
        }
        if (auto const colon = host.find(':'); colon != std::string::npos) {
            if (host.empty() || host.front() != '[') {
                host = host.substr(0, colon);
            }
        }
        host = geode::utils::string::toLower(std::move(host));
        if (!isAllowedDiscordWebhookHost(host)) {
            log::warn(
                "Webhook URL host must be discord.com, discordapp.com, "
                "canary.discord.com, or ptb.discord.com"
            );
            return std::nullopt;
        }
        return url;
    }

    std::vector<std::string> collectWebhookTargets() {
        static constexpr char const* kWebhookKeys[] = {
            "webhook-url",
            "extra-webhook-url-1",
            "extra-webhook-url-2",
            "extra-webhook-url-3",
            "extra-webhook-url-4",
        };
        std::vector<std::string> out;
        for (auto* key : kWebhookKeys) {
            if (auto u = normalizeWebhookUrl(Mod::get()->getSettingValue<std::string>(key))) {
                out.push_back(std::move(*u));
            }
        }
        return out;
    }

    std::string currentIso8601Utc() {
        auto const now = asp::SystemTime::now().dateTimeUtc();
        return fmt::format(
            "{:04}-{:02}-{:02}T{:02}:{:02}:{:02}Z",
            now.date.year,
            static_cast<int>(now.date.month),
            static_cast<int>(now.date.day),
            static_cast<int>(now.time.hours),
            static_cast<int>(now.time.minutes),
            static_cast<int>(now.time.seconds)
        );
    }

    matjson::Value buildWebhookPayload(
        std::string const& title, std::string const& description, int color,
        std::vector<WebhookField> const& fields, std::string const& footer,
        bool embedScreenshotAttachment
    ) {
        auto const titleClamped =
            clampUtf8ByBytes(title, kDiscordEmbedTitleMax, "webhook embed title");
        auto const descClamped =
            clampUtf8ByBytes(description, kDiscordEmbedDescriptionMax, "webhook embed description");
        auto const footerClamped =
            clampUtf8ByBytes(footer, kDiscordEmbedFooterMax, "webhook embed footer");

        auto fieldsArr = matjson::Value::array();
        size_t const nFields = std::min(fields.size(), kDiscordEmbedFieldCountMax);
        if (fields.size() > kDiscordEmbedFieldCountMax) {
            log::warn(
                "Webhook embed fields truncated from {} to {}", fields.size(), kDiscordEmbedFieldCountMax
            );
        }
        for (size_t i = 0; i < nFields; ++i) {
            auto const& f = fields[i];
            auto obj = matjson::Value::object();
            obj["name"] =
                clampUtf8ByBytes(f.name, kDiscordEmbedFieldNameMax, "webhook embed field name");
            obj["value"] =
                clampUtf8ByBytes(f.value, kDiscordEmbedFieldValueMax, "webhook embed field value");
            obj["inline"] = f.inlineField;
            fieldsArr.push(obj);
        }

        auto embed = matjson::Value::object();
        embed["title"] = titleClamped;
        embed["description"] = descClamped;
        embed["color"] = color;
        embed["fields"] = fieldsArr;
        embed["timestamp"] = currentIso8601Utc();
        if (!footerClamped.empty()) {
            auto footerObj = matjson::Value::object();
            footerObj["text"] = footerClamped;
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
        auto username =
            geode::utils::string::trim(Mod::get()->getSettingValue<std::string>("webhook-username"));
        if (!username.empty()) {
            payload["username"] = clampUtf8ByBytes(
                std::move(username), kDiscordWebhookUsernameMax, "webhook username override"
            );
        }
        payload["embeds"] = embedsArr;
        return payload;
    }

    std::optional<int> backoffSecondsForFailedAttempt(
        web::WebResponse const& res, int attempt, int maxRetries,
        std::optional<int> maxDelaySeconds = std::nullopt
    ) {
        auto const retryAfter = res.header("Retry-After");
        auto const delay = retry_policy::delayForFailure(
            res.code(),
            retryAfter ? std::optional<std::string_view>{*retryAfter} : std::nullopt,
            attempt,
            maxRetries,
            maxDelaySeconds
        );
        if (!delay) {
            auto const error = safeWebErrorMessage(res);
            log::warn(
                "Webhook POST failed after {} attempts (status {}){}{}",
                attempt + 1,
                res.code(),
                error.empty() ? "" : ": ",
                error
            );
            return std::nullopt;
        }
        if (res.code() != 429) {
            auto const error = safeWebErrorMessage(res);
            log::warn(
                "Webhook POST failed (status {}){}{}, retrying in {}s ({}/{})",
                res.code(),
                error.empty() ? "" : ": ",
                error,
                *delay,
                attempt + 1,
                maxRetries + 1
            );
        }
        return delay;
    }

    void postWebhookSyncWithRetries(std::string const& url, matjson::Value const& payload, int maxRetries) {
        for (int attempt = 0;; ++attempt) {
            auto req = web::WebRequest();
            req.bodyJSON(payload);
            req.timeout(kSyncRequestTimeout);

            auto res = req.postSync(url);
            if (res.ok()) {
                return;
            }
            auto wait =
                backoffSecondsForFailedAttempt(res, attempt, maxRetries, kSyncMaxRetryDelaySeconds);
            if (!wait) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::seconds(*wait));
        }
    }

    arc::Future<> postWebhookWithRetries(std::string url, matjson::Value payload, int maxRetries) {
        for (int attempt = 0;; ++attempt) {
            auto req = web::WebRequest();
            req.bodyJSON(payload);
            req.timeout(kAsyncRequestTimeout);

            auto res = co_await req.post(url);
            if (res.ok()) {
                co_return;
            }
            auto wait = backoffSecondsForFailedAttempt(res, attempt, maxRetries);
            if (!wait) {
                co_return;
            }
            co_await arc::sleep(asp::Duration::fromSecs(*wait));
        }
    }

    arc::Future<> postWebhookMultipartWithRetries(
        std::string url, std::string payloadJson,
        std::shared_ptr<std::vector<std::uint8_t> const> pngBytes, int maxRetries
    ) {
        if (!pngBytes) {
            co_return;
        }
        for (int attempt = 0;; ++attempt) {
            web::MultipartForm form;
            form.param("payload_json", payloadJson);
            form.file("files[0]", *pngBytes, "screenshot.png", "image/png");
            auto req = web::WebRequest();
            req.bodyMultipart(std::move(form));
            req.timeout(kAsyncRequestTimeout);

            auto res = co_await req.post(url);
            if (res.ok()) {
                co_return;
            }
            auto wait = backoffSecondsForFailedAttempt(res, attempt, maxRetries);
            if (!wait) {
                co_return;
            }
            co_await arc::sleep(asp::Duration::fromSecs(*wait));
        }
    }

    void postWebhookSyncMultipartWithRetries(
        std::string const& url, std::string const& payloadJson,
        std::vector<std::uint8_t> const& pngBytes, int maxRetries
    ) {
        for (int attempt = 0;; ++attempt) {
            web::MultipartForm form;
            form.param("payload_json", payloadJson);
            form.file("files[0]", pngBytes, "screenshot.png", "image/png");
            auto req = web::WebRequest();
            req.bodyMultipart(std::move(form));
            req.timeout(kSyncRequestTimeout);

            auto res = req.postSync(url);
            if (res.ok()) {
                return;
            }
            auto wait =
                backoffSecondsForFailedAttempt(res, attempt, maxRetries, kSyncMaxRetryDelaySeconds);
            if (!wait) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::seconds(*wait));
        }
    }

    void sendImpl(bool useSync, WebhookMessage message) {
        auto urls = collectWebhookTargets();
        if (urls.empty()) {
            return;
        }
        auto maxRetries = static_cast<int>(Mod::get()->getSettingValue<int64_t>("max-retries"));
        if (maxRetries < 0) {
            maxRetries = 0;
        }
        int const effectiveMaxRetries = useSync ? std::min(maxRetries, kSyncMaxRetries) : maxRetries;

        bool const hasShot = message.screenshotPng.has_value() && !message.screenshotPng->empty();

        if (hasShot) {
            auto payload = buildWebhookPayload(
                message.title, message.description, message.color, message.fields, message.footer, true
            );
            auto payloadJson = payload.dump(matjson::NO_INDENTATION);
            if (useSync) {
                auto const& bytes = *message.screenshotPng;
                for (auto const& url : urls) {
                    postWebhookSyncMultipartWithRetries(url, payloadJson, bytes, effectiveMaxRetries);
                }
            }
            else {
                auto sharedBytes = std::make_shared<std::vector<std::uint8_t> const>(
                    std::move(*message.screenshotPng)
                );
                for (auto const& url : urls) {
                    async::spawn(postWebhookMultipartWithRetries(
                        url, payloadJson, sharedBytes, effectiveMaxRetries
                    ));
                }
            }
            return;
        }

        auto payload = buildWebhookPayload(
            message.title, message.description, message.color, message.fields, message.footer, false
        );
        if (useSync) {
            for (auto const& url : urls) {
                postWebhookSyncWithRetries(url, matjson::Value(payload), effectiveMaxRetries);
            }
        }
        else {
            for (auto const& url : urls) {
                async::spawn(postWebhookWithRetries(url, matjson::Value(payload), effectiveMaxRetries));
            }
        }
    }

    matjson::Value buildContentWebhookPayload(std::string const& content) {
        auto payload = matjson::Value::object();
        auto username =
            geode::utils::string::trim(Mod::get()->getSettingValue<std::string>("webhook-username"));
        if (!username.empty()) {
            payload["username"] = clampUtf8ByBytes(
                std::move(username), kDiscordWebhookUsernameMax, "webhook username override"
            );
        }
        payload["content"] = content;
        return payload;
    }

    void sendContentImpl(std::string const& content) {
        auto urls = collectWebhookTargets();
        if (urls.empty()) {
            return;
        }
        auto maxRetries = static_cast<int>(Mod::get()->getSettingValue<int64_t>("max-retries"));
        if (maxRetries < 0) {
            maxRetries = 0;
        }
        auto base = buildContentWebhookPayload(content);
        for (auto const& url : urls) {
            async::spawn(postWebhookWithRetries(url, matjson::Value(base), maxRetries));
        }
    }

} // namespace webhook_impl

void sendWebhookContent(std::string const& content) {
    constexpr size_t kMaxDiscordContent = 2000;
    webhook_impl::sendContentImpl(
        webhook_impl::clampUtf8ByBytes(content, kMaxDiscordContent, "webhook message content")
    );
}

void sendWebhook(WebhookMessage message) {
    webhook_impl::sendImpl(false, std::move(message));
}

void sendWebhookIfEnabled(std::string const& settingKey, WebhookMessage message) {
    if (!Mod::get()->getSettingValue<bool>(settingKey)) {
        return;
    }
    sendWebhook(std::move(message));
}

void sendWebhookBlocking(WebhookMessage message) {
    webhook_impl::sendImpl(true, std::move(message));
}
