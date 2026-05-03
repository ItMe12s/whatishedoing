#include "custom_text.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/file.hpp>

#include <filesystem>
#include <system_error>

using namespace geode::prelude;

namespace level_upload {

namespace {

char const* kDefaultTemplate = R"(## {isUploaded"New Level!"}{isUpdated"Level Updated!"}
**{creator} {isUploaded"dropped a new"}{isUpdated"updated a"} level!**
- Name: {name}
- ID: {id}
-# {lengh} ({objects} objects)
||{role}||)";

} // namespace

std::filesystem::path customTextFilePath() {
    return Mod::get()->getConfigDir() / "customtext.txt";
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

} // namespace level_upload
