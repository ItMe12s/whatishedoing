#include "message.hpp"

#include "custom_text.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/utils/general.hpp>
#include <Geode/utils/string.hpp>
#include <array>
#include <string_view>
#include <utility>

using namespace geode::prelude;

namespace level_upload {
    namespace {

        std::string lengthString(int len) {
            static constexpr auto kLengths =
                std::to_array<std::string_view>({"Tiny", "Short", "Medium", "Long", "XL", "Plat"});
            return std::string(
                (len >= 0 && std::cmp_less(len, kLengths.size())) ? kLengths[len] : "Unknown"
            );
        }

        std::string processConditionals(std::string text, bool isUpdate) {
            auto process = [&](std::string& str, std::string const& tag, bool show) {
                std::string open = "{" + tag + "\"";
                size_t pos = 0;
                while ((pos = str.find(open, pos)) != std::string::npos) {
                    size_t contentStart = pos + open.size();
                    size_t closing = str.find("\"}", contentStart);
                    if (closing == std::string::npos) break;
                    std::string inner = str.substr(contentStart, closing - contentStart);
                    std::string replacement = show ? inner : "";
                    str.replace(pos, closing + 2 - pos, replacement);
                    pos += replacement.size();
                }
            };
            process(text, "isUploaded", !isUpdate);
            process(text, "isUpdated", isUpdate);
            return text;
        }

    } // namespace

    std::string buildUploadMessage(GJGameLevel* level, bool isUpdate) {
        auto mod = Mod::get();
        bool rolePing = mod->getSettingValue<bool>("upload-role-ping");
        std::string roleID = mod->getSettingValue<std::string>("upload-role-id");
        geode::utils::string::trimIP(roleID);
        bool const wantRolePing = rolePing && !roleID.empty();
        std::string creator = level->m_creatorName;
        std::string name = level->m_levelName;
        std::string id = geode::utils::numToString(static_cast<int>(level->m_levelID));
        std::string length = lengthString(level->m_levelLength);
        std::string objects = geode::utils::numToString(static_cast<int>(level->m_objectCount));
        std::string text;

        if (mod->getSettingValue<bool>("upload-use-custom-text")) {
            text = readCustomTextFile();
            for (auto [key, value] : std::array{
                     std::pair<std::string_view, std::string>{"{creator}", creator},
                     std::pair<std::string_view, std::string>{"{name}", name},
                     std::pair<std::string_view, std::string>{"{id}", id},
                     std::pair<std::string_view, std::string>{"{length}", length},
                     std::pair<std::string_view, std::string>{"{objects}", objects},
                     std::pair<std::string_view, std::string>{
                         "{role}", wantRolePing ? fmt::format("<@&{}>", roleID) : ""
                     },
                 }) {
                geode::utils::string::replaceIP(text, key, value);
            }
            text = processConditionals(text, isUpdate);
        }
        else {
            text = isUpdate ?
                fmt::format(
                    "**{}** updated **{}**!\n- ID: {} • {} • {} objects", creator, name, id, length, objects
                ) :
                fmt::format(
                    "**{}** dropped a new level **{}**!\n- ID: {} • {} • {} objects",
                    creator,
                    name,
                    id,
                    length,
                    objects
                );
            if (wantRolePing) text += fmt::format("\n||<@&{}>||", roleID);
        }
        return text;
    }

} // namespace level_upload
