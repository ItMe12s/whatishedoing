#include "open_file_setting.hpp"

#include "custom_text.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/file.hpp>

#ifdef GEODE_IS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

#include "state.hpp"

using namespace geode::prelude;

namespace level_upload {

void openCustomTextFileFromSettings() {
    ensureDefaultCustomTextFile();
    auto path = customTextFilePath();
#ifdef GEODE_IS_WINDOWS
    ShellExecuteW(nullptr, L"open", L"notepad.exe", path.wstring().c_str(), nullptr, SW_SHOW);
#else
    utils::file::openFolder(Mod::get()->getConfigDir());
#endif
    auto& session = gameSession();
    if (session.eventCount < session.trackedEvents.size()) {
        session.trackedEvents[session.eventCount++] = Tracked::CustomTextEdit;
    }
}

} // namespace level_upload
