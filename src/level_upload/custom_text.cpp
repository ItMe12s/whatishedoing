#include "custom_text.hpp"

#include "profile/data.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/file.hpp>
#include <filesystem>
#include <system_error>

using namespace geode::prelude;

namespace level_upload {

    namespace {

        char const* kDefaultTemplate =
            R"(## {isUploaded"New Level!"}{isUpdated"Level Updated!"}
**{creator} {isUploaded"dropped a new"}{isUpdated"updated a"} level!**
- Name: {name}
- ID: {id}
-# {lengh} ({objects} objects)
||{role}||)";

        std::filesystem::path customTextFilePath() {
            auto const n = profile::activeCustomTextSlotIndex() + 1;
            return Mod::get()->getConfigDir() / fmt::format("customtextprofile{}.txt", n);
        }

        Result<> ensureDefaultCustomTextFile() {
            auto const path = customTextFilePath();
            std::error_code ec;
            if (std::filesystem::exists(path, ec)) {
                return Ok();
            }
            if (ec) {
                return Err("Could not inspect custom text file: {}", ec.message());
            }
            return file::writeStringSafe(path, kDefaultTemplate);
        }

    } // namespace

    std::string readCustomTextFile() {
        if (auto res = ensureDefaultCustomTextFile(); res.isErr()) {
            log::error("Could not create custom text template: {}", res.unwrapErr());
            return std::string(kDefaultTemplate);
        }
        return file::readString(customTextFilePath()).unwrapOr(kDefaultTemplate);
    }

    void revealCustomTextFileFromSettings() {
        if (auto res = ensureDefaultCustomTextFile(); res.isErr()) {
            Notification::create(
                fmt::format("Could not create template: {}", res.unwrapErr()),
                NotificationIcon::Error,
                2.f
            )
                ->show();
            return;
        }
        if (!file::openFolder(customTextFilePath())) {
            Notification::create("Could not reveal template file", NotificationIcon::Error, 2.f)->show();
        }
    }

} // namespace level_upload
