#include "data.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/string.hpp>
#include <algorithm>
#include <cstdint>

using namespace geode::prelude;

namespace profile {

namespace {

constexpr char const* kProfileDataKey = "profile-data";
constexpr char const* kProfileNamesKey = "profile-names";
constexpr std::size_t kMaxNameLength = 32;

std::array<Tracked, 24> const kTracked{{
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
    {"notify-new-best", Kind::Bool},
    {"notify-editor", Kind::Bool},
    {"notify-death", Kind::Bool},
    {"death-min-percent", Kind::Int},
    {"startpos-death-min-progress", Kind::Int},
    {"level-filter-mode", Kind::String},
    {"level-filter-ids", Kind::String},
    {"suppress-redacted", Kind::Bool},
    {"screenshot-level-complete", Kind::Bool},
    {"screenshot-new-best", Kind::Bool},
    {"screenshot-death", Kind::Bool},
    {"screenshot-scale-percent", Kind::Int},
}};

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
    auto arr = matjson::Value::array();
    for (auto const& n : names) {
        arr.push(n);
    }
    Mod::get()->setSavedValue<matjson::Value>(kProfileNamesKey, arr);
}

matjson::Value snapshotCurrentSettings() {
    auto out = matjson::Value::object();
    for (auto const& t : kTracked) {
        switch (t.kind) {
            case Kind::Bool:
                out[t.key] = Mod::get()->getSettingValue<bool>(t.key);
                break;
            case Kind::Int:
                out[t.key] = Mod::get()->getSettingValue<int64_t>(t.key);
                break;
            case Kind::String:
                out[t.key] =
                    Mod::get()->getSettingValue<std::string>(t.key);
                break;
        }
    }
    return out;
}

void applyBlobToSettings(matjson::Value const& blob) {
    if (!blob.isObject()) return;
    for (auto const& t : kTracked) {
        if (!blob.contains(t.key)) continue;
        auto const& v = blob[t.key];
        switch (t.kind) {
            case Kind::Bool: {
                auto r = v.asBool();
                if (r.isOk()) {
                    Mod::get()->setSettingValue<bool>(t.key, r.unwrap());
                }
                break;
            }
            case Kind::Int: {
                auto r = v.asInt();
                if (r.isOk()) {
                    Mod::get()->setSettingValue<int64_t>(
                        t.key,
                        static_cast<int64_t>(r.unwrap())
                    );
                }
                break;
            }
            case Kind::String: {
                auto r = v.asString();
                if (r.isOk()) {
                    Mod::get()->setSettingValue<std::string>(
                        t.key,
                        r.unwrap()
                    );
                }
                break;
            }
        }
    }
}

} // namespace

std::array<Tracked, 24> const& trackedKeys() {
    return kTracked;
}

std::array<std::string, kSlotCount> slotNames() {
    std::array<std::string, kSlotCount> out;
    auto raw = loadNamesRaw();
    for (std::size_t i = 0; i < kSlotCount; ++i) {
        std::string name;
        if (raw.isArray() && i < raw.size()) {
            auto r = raw[i].asString();
            if (r.isOk()) name = r.unwrap();
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
    for (std::size_t i = 0; i < kSlotCount; ++i) {
        if (i == idx) continue;
        if (names[i] == trimmed) {
            return Err("Another slot is already named '{}'", trimmed);
        }
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
        log::warn(
            "applyProfileNow: saveData failed: {}",
            res.unwrapErr()
        );
    }
    log::info("Applied profile '{}'", slot);
    return true;
}

} // namespace profile
