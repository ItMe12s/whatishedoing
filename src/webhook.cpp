#include "webhook.hpp"

#include "retry_policy.hpp"

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

    static Result<std::string> truncateDiscordText(
        std::string_view text, size_t maxCharacters, char const* context
    ) {
        if (text.empty()) {
            return Ok(std::string());
        }
        auto decoded = geode::utils::string::utf8ToUtf32(text);
        if (decoded.isErr()) {
            log::error("Invalid UTF-8 in {}, webhook request skipped", context);
            return Err("Invalid UTF-8");
        }
        auto codePoints = std::move(decoded).unwrap();
        if (codePoints.size() <= maxCharacters) {
            return Ok(std::string(text));
        }
        log::warn("{} truncated from {} to {} characters", context, codePoints.size(), maxCharacters);
        codePoints.resize(maxCharacters);
        return geode::utils::string::utf32ToUtf8(codePoints);
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
        static auto const kSecretMarkers =
            std::to_array<std::string>({"://", "/api/webhooks/", "\r", "\n"});
        auto const error = res.errorMessage();
        if (error.empty() || geode::utils::string::containsAny(error, kSecretMarkers)) {
            return {};
        }
        return error;
    }

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

    int maxRetriesSetting() {
        return std::max(0, static_cast<int>(Mod::get()->getSettingValue<int64_t>("max-retries")));
    }

    Result<> applyUsernameOverride(matjson::Value& payload) {
        auto username =
            geode::utils::string::trim(Mod::get()->getSettingValue<std::string>("webhook-username"));
        if (username.empty()) {
            return Ok();
        }
        GEODE_UNWRAP_INTO(
            auto truncated,
            truncateDiscordText(username, kDiscordWebhookUsernameMax, "webhook username override")
        );
        payload["username"] = std::move(truncated);
        return Ok();
    }

    Result<matjson::Value> buildWebhookPayload(
        std::string const& title, std::string const& description, int color,
        std::vector<WebhookField> const& fields, std::string const& footer,
        bool embedScreenshotAttachment
    ) {
        GEODE_UNWRAP_INTO(
            auto titleClamped,
            truncateDiscordText(title, kDiscordEmbedTitleMax, "webhook embed title")
        );
        GEODE_UNWRAP_INTO(
            auto descClamped,
            truncateDiscordText(description, kDiscordEmbedDescriptionMax, "webhook embed description")
        );
        GEODE_UNWRAP_INTO(
            auto footerClamped,
            truncateDiscordText(footer, kDiscordEmbedFooterMax, "webhook embed footer")
        );

        auto fieldsArr = matjson::Value::array();
        size_t const nFields = std::min(fields.size(), kDiscordEmbedFieldCountMax);
        if (fields.size() > kDiscordEmbedFieldCountMax) {
            log::warn(
                "Webhook embed fields truncated from {} to {}", fields.size(), kDiscordEmbedFieldCountMax
            );
        }
        for (size_t i = 0; i < nFields; ++i) {
            auto const& f = fields[i];
            GEODE_UNWRAP_INTO(
                auto name,
                truncateDiscordText(f.name, kDiscordEmbedFieldNameMax, "webhook embed field name")
            );
            GEODE_UNWRAP_INTO(
                auto value,
                truncateDiscordText(f.value, kDiscordEmbedFieldValueMax, "webhook embed field value")
            );
            auto obj = matjson::Value::object();
            obj["name"] = std::move(name);
            obj["value"] = std::move(value);
            obj["inline"] = f.inlineField;
            fieldsArr.push(obj);
        }

        auto embed = matjson::Value::object();
        embed["title"] = std::move(titleClamped);
        embed["description"] = std::move(descClamped);
        embed["color"] = color;
        embed["fields"] = fieldsArr;
        embed["timestamp"] = currentIso8601Utc();
        if (!footerClamped.empty()) {
            auto footerObj = matjson::Value::object();
            footerObj["text"] = std::move(footerClamped);
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
        GEODE_UNWRAP(applyUsernameOverride(payload));
        payload["embeds"] = embedsArr;
        return Ok(std::move(payload));
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

    void postSyncWithRetries(std::string const& url, int maxRetries, auto&& setupRequest) {
        for (int attempt = 0;; ++attempt) {
            auto req = web::WebRequest();
            setupRequest(req);
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

    arc::Future<> postAsyncWithRetries(std::string const& url, int maxRetries, auto setupRequest) {
        for (int attempt = 0;; ++attempt) {
            auto req = web::WebRequest();
            setupRequest(req);
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

    void sendImpl(bool useSync, WebhookMessage message) {
        auto urls = collectWebhookTargets();
        if (urls.empty()) {
            return;
        }
        bool const hasShot = message.screenshotPng.has_value() && !message.screenshotPng->empty();
        auto payloadResult = buildWebhookPayload(
            message.title, message.description, message.color, message.fields, message.footer, hasShot
        );
        if (payloadResult.isErr()) {
            return;
        }
        auto payload = std::move(payloadResult).unwrap();
        int const maxRetries = maxRetriesSetting();
        int const effectiveMaxRetries = useSync ? std::min(maxRetries, kSyncMaxRetries) : maxRetries;

        if (hasShot) {
            auto payloadJson = payload.dump(matjson::NO_INDENTATION);
            if (useSync) {
                auto const& bytes = *message.screenshotPng;
                for (auto const& url : urls) {
                    postSyncWithRetries(url, effectiveMaxRetries, [&](web::WebRequest& req) {
                        web::MultipartForm form;
                        form.param("payload_json", payloadJson);
                        form.file("files[0]", bytes, "screenshot.png", "image/png");
                        req.bodyMultipart(std::move(form));
                    });
                }
            }
            else {
                auto sharedBytes = std::make_shared<std::vector<std::uint8_t> const>(
                    std::move(*message.screenshotPng)
                );
                for (auto const& url : urls) {
                    async::spawn(postAsyncWithRetries(
                        url, effectiveMaxRetries, [payloadJson, sharedBytes](web::WebRequest& req) {
                            web::MultipartForm form;
                            form.param("payload_json", payloadJson);
                            form.file("files[0]", *sharedBytes, "screenshot.png", "image/png");
                            req.bodyMultipart(std::move(form));
                        }
                    ));
                }
            }
            return;
        }

        if (useSync) {
            for (auto const& url : urls) {
                postSyncWithRetries(url, effectiveMaxRetries, [&payload](web::WebRequest& req) {
                    req.bodyJSON(payload);
                });
            }
        }
        else {
            for (auto const& url : urls) {
                async::spawn(postAsyncWithRetries(
                    url, effectiveMaxRetries, [payload = matjson::Value(payload)](web::WebRequest& req) {
                        req.bodyJSON(payload);
                    }
                ));
            }
        }
    }

} // namespace webhook_impl

void sendWebhookContent(std::string const& content) {
    constexpr size_t kMaxDiscordContent = 2000;
    auto urls = webhook_impl::collectWebhookTargets();
    if (urls.empty()) {
        return;
    }
    auto contentResult =
        webhook_impl::truncateDiscordText(content, kMaxDiscordContent, "webhook message content");
    if (contentResult.isErr()) {
        return;
    }
    auto payload = matjson::Value::object();
    payload["content"] = std::move(contentResult).unwrap();
    if (webhook_impl::applyUsernameOverride(payload).isErr()) {
        return;
    }
    int const maxRetries = webhook_impl::maxRetriesSetting();
    for (auto const& url : urls) {
        async::spawn(
            webhook_impl::postAsyncWithRetries(
                url, maxRetries, [payload = matjson::Value(payload)](web::WebRequest& req) {
                    req.bodyJSON(payload);
                }
            )
        );
    }
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
