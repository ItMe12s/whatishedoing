#pragma once

#include <filesystem>
#include <string>

namespace level_upload {

    std::filesystem::path customTextFilePath();
    void ensureDefaultCustomTextFile();
    std::string readCustomTextFile();
    void openCustomTextFileFromSettings();

} // namespace level_upload
