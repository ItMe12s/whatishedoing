#include "custom_text.hpp"

#include "profile/data.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/file.hpp>
#include <filesystem>
#include <system_error>

#ifdef GEODE_IS_WINDOWS
    #define WIN32_LEAN_AND_MEAN
// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on
#endif

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

    } // namespace

    std::filesystem::path customTextFilePath() {
        auto const n = profile::activeCustomTextSlotIndex() + 1;
        return Mod::get()->getConfigDir() / fmt::format("customtextprofile{}.txt", n);
    }

    void ensureDefaultCustomTextFile() {
        auto const path = customTextFilePath();
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            (void)geode::utils::file::writeString(path, kDefaultTemplate);
        }
    }

    std::string readCustomTextFile() {
        ensureDefaultCustomTextFile();
        auto const path = customTextFilePath();
        auto res = geode::utils::file::readString(path);
        if (res.isOk()) {
            return res.unwrap();
        }
        return std::string(kDefaultTemplate);
    }

    void openCustomTextFileFromSettings() {
        ensureDefaultCustomTextFile();
        auto const path = customTextFilePath();
#ifdef GEODE_IS_WINDOWS
        ShellExecuteW(nullptr, L"open", L"notepad.exe", path.wstring().c_str(), nullptr, SW_SHOW);
#else
        utils::file::openFolder(Mod::get()->getConfigDir());
#endif
    }

} // namespace level_upload
