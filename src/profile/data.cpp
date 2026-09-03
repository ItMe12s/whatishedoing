#include "data.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/ranges.hpp>
#include <Geode/utils/string.hpp>
#include <cstdint>
#include <ranges>

using namespace geode::prelude;

namespace profile {

    namespace {

        constexpr char const* kProfileDataKey = "profile-data";
        constexpr char const* kProfileNamesKey = "profile-names";
        constexpr char const* kActiveCustomTextSlotKey = "active-custom-text-slot";
        constexpr std::size_t kMaxNameLength = 32;

        enum class Kind {
            Bool,
            Int,
            Float,
            String,
            Color
        };

        struct TrackedKey {
            char const* key;
            Kind kind;
        };

        constexpr auto kTracked = std::to_array<TrackedKey>({
            {"webhook-url", Kind::String},
            {"extra-webhook-url-1", Kind::String},
            {"extra-webhook-url-2", Kind::String},
            {"extra-webhook-url-3", Kind::String},
            {"extra-webhook-url-4", Kind::String},
            {"webhook-username", Kind::String},
            {"player-name", Kind::String},
            {"max-retries", Kind::Int},
            {"blocking-webhook", Kind::Bool},
            {"notify-game-session", Kind::Bool},
            {"notify-play-level", Kind::Bool},
            {"notify-level-complete", Kind::Bool},
            {"notify-practice-complete", Kind::Bool},
            {"notify-new-best", Kind::Bool},
            {"notify-editor", Kind::Bool},
            {"notify-death", Kind::Bool},
            {"cheat-detect", Kind::Bool},
            {"death-min-percent", Kind::Int},
            {"startpos-death-min-progress", Kind::Int},
            {"new-best-min-percent", Kind::Int},
            {"level-filter-mode", Kind::String},
            {"level-filter-ids", Kind::String},
            {"suppress-redacted", Kind::Bool},
            {"screenshot-level-complete", Kind::Bool},
            {"screenshot-new-best", Kind::Bool},
            {"screenshot-death", Kind::Bool},
            {"screenshot-scale-percent", Kind::Int},
            {"screenshot-delay", Kind::Float},
            {"notify-level-upload", Kind::Bool},
            {"upload-send-on-update", Kind::Bool},
            {"upload-use-custom-text", Kind::Bool},
            {"upload-role-ping", Kind::Bool},
            {"upload-role-id", Kind::String},
            {"color-game-open", Kind::Color},
            {"color-game-close", Kind::Color},
            {"color-test-webhook", Kind::Color},
            {"color-editor-open", Kind::Color},
            {"color-editor-exit", Kind::Color},
            {"color-new-best", Kind::Color},
            {"color-level-complete", Kind::Color},
            {"color-level-exit", Kind::Color},
            {"color-death", Kind::Color},
            {"color-play-practice", Kind::Color},
            {"color-play-normal", Kind::Color},
        });

        std::string defaultNameFor(std::size_t idx) {
            return fmt::format("Profile {}", idx + 1);
        }

        matjson::Value loadAll() {
            auto v = Mod::get()->getSavedValue<matjson::Value>(kProfileDataKey);
            if (!v.isObject()) return matjson::Value::object();
            return v;
        }

        void storeAll(matjson::Value const& v) {
            Mod::get()->setSavedValue<matjson::Value>(kProfileDataKey, v);
        }

        matjson::Value loadNamesRaw() {
            auto v = Mod::get()->getSavedValue<matjson::Value>(kProfileNamesKey);
            if (!v.isArray()) return matjson::Value::array();
            return v;
        }

        void storeNames(std::array<std::string, kSlotCount> const& names) {
            Mod::get()->setSavedValue(kProfileNamesKey, names);
        }

        matjson::Value snapshotCurrentSettings() {
            auto out = matjson::Value::object();
            for (auto const& t : kTracked) {
                matjson::Value value;
                if (auto setting = Mod::get()->getSetting(t.key); setting && setting->save(value)) {
                    out[t.key] = std::move(value);
                }
            }
            return out;
        }

        template <class T, class Parse>
        void setIfOk(matjson::Value const& v, char const* key, Parse parse) {
            if (auto r = parse(v); r.isOk()) {
                Mod::get()->setSettingValue<T>(key, r.unwrap());
            }
        }

        void applyBlobToSettings(matjson::Value const& blob) {
            if (!blob.isObject()) return;
            for (auto const& t : kTracked) {
                if (!blob.contains(t.key)) continue;
                auto const& v = blob[t.key];
                switch (t.kind) {
                    case Kind::Bool:
                        setIfOk<bool>(v, t.key, [](auto& v) {
                            return v.asBool();
                        });
                        break;
                    case Kind::Int:
                        setIfOk<int64_t>(v, t.key, [](auto& v) {
                            return v.asInt();
                        });
                        break;
                    case Kind::Float:
                        setIfOk<double>(v, t.key, [](auto& v) {
                            return v.asDouble();
                        });
                        break;
                    case Kind::String:
                        setIfOk<std::string>(v, t.key, [](auto& v) {
                            return v.asString();
                        });
                        break;
                    case Kind::Color:
                        setIfOk<cocos2d::ccColor3B>(v, t.key, [](auto& v) {
                            return v.template as<cocos2d::ccColor3B>();
                        });
                        break;
                }
            }
        }

    } // namespace

    std::array<std::string, kSlotCount> slotNames() {
        std::array<std::string, kSlotCount> out;
        auto raw = loadNamesRaw();
        for (auto i : std::views::iota(std::size_t{0}, kSlotCount)) {
            std::string name;
            if (raw.isArray() && i < raw.size()) {
                name = raw[i].asString().unwrapOrDefault();
            }
            if (name.empty()) name = defaultNameFor(i);
            out[i] = std::move(name);
        }
        return out;
    }

    std::string slotNameAt(std::size_t idx) {
        if (idx >= kSlotCount) return {};
        return slotNames()[idx];
    }

    bool slotIsFilled(std::string const& slot) {
        auto all = loadAll();
        return all.contains(slot) && all[slot].isObject();
    }

    void snapshotIntoSlot(std::string const& slot) {
        auto all = loadAll();
        all[slot] = snapshotCurrentSettings();
        storeAll(all);
    }

    void clearSlot(std::string const& slot) {
        auto all = loadAll();
        if (!all.contains(slot)) return;
        all.erase(slot);
        storeAll(all);
    }

    Result<> renameSlot(std::size_t idx, std::string newName) {
        if (idx >= kSlotCount) {
            return Err("Invalid slot index");
        }
        auto trimmed = geode::utils::string::trim(std::move(newName));
        if (trimmed.empty()) {
            return Err("Name cannot be empty");
        }
        if (trimmed.size() > kMaxNameLength) {
            return Err("Name too long (max {} chars)", kMaxNameLength);
        }
        auto names = slotNames();
        auto const old = names[idx];
        if (trimmed == old) {
            return Ok();
        }
        if (geode::utils::ranges::contains(names, trimmed)) {
            return Err("Another slot is already named '{}'", trimmed);
        }

        auto all = loadAll();
        if (all.contains(old) && all[old].isObject()) {
            all[trimmed] = all[old];
            all.erase(old);
            storeAll(all);
        }

        names[idx] = trimmed;
        storeNames(names);
        return Ok();
    }

    bool applyProfileNow(std::string const& slot) {
        auto all = loadAll();
        if (!all.contains(slot) || !all[slot].isObject()) {
            log::warn("applyProfileNow: slot '{}' not found", slot);
            return false;
        }
        applyBlobToSettings(all[slot]);
        auto res = Mod::get()->saveData();
        if (res.isErr()) {
            log::warn("applyProfileNow: saveData failed: {}", res.unwrapErr());
            return false;
        }
        log::info("Applied profile '{}'", slot);
        return true;
    }

    std::size_t activeCustomTextSlotIndex() {
        int64_t const raw = Mod::get()->getSavedValue<int64_t>(kActiveCustomTextSlotKey);
        return std::in_range<std::size_t>(raw) && static_cast<std::size_t>(raw) < kSlotCount ?
            static_cast<std::size_t>(raw) :
            0;
    }

    void setActiveCustomTextSlotIndex(std::size_t idx) {
        if (idx >= kSlotCount) {
            idx = 0;
        }
        Mod::get()->setSavedValue<int64_t>(kActiveCustomTextSlotKey, static_cast<int64_t>(idx));
        (void)Mod::get()->saveData();
    }

} // namespace profile
