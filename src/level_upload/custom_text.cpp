#include "custom_text.hpp"

#include "profile/data.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/file.hpp>

using namespace geode::prelude;

namespace level_upload {

    namespace {

        char const* kDefaultTemplate =
            R"(## {isUploaded"New Level!"}{isUpdated"Level Updated!"}
**{creator} {isUploaded"dropped a new"}{isUpdated"updated a"} level!**
- Name: {name}
- ID: {id}
-# {length} ({objects} objects)
||{role}||)";

        asp::fs::Path customTextFilePath() {
            auto const n = profile::activeCustomTextSlotIndex() + 1;
            return Mod::get()->getConfigDir() / fmt::format("customtextprofile{}.txt", n);
        }

        Result<> ensureDefaultCustomTextFile() {
            auto const path = customTextFilePath();
            if (path.exists()) {
                return Ok();
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
