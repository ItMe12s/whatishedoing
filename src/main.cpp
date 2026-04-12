#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/binding/GJGameLevel.hpp>

#include "webhook.hpp"

using namespace geode::prelude;

class $modify(MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        static bool s_sentOpen = false;
        if (!s_sentOpen) {
            s_sentOpen = true;
            sendWebhook(
                "notify-game-open",
                "Opened Geometry Dash",
                "Just opened Geometry Dash!",
                3447003
            );
        }

        return true;
    }
};

class $modify(MyPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        auto levelName   = std::string(level->m_levelName);
        auto creatorName = std::string(level->m_creatorName);
        auto levelID     = std::to_string(level->m_levelID.value());

        sendWebhook(
            "notify-play-level",
            "Playing a Level",
            fmt::format("Now playing **{}** by **{}**", levelName, creatorName),
            5763719,
            {
                { "Level", levelName, true },
                { "Creator", creatorName.empty() ? "Unknown" : creatorName, true },
                { "Level ID", levelID, true }
            }
        );

        return true;
    }

    void levelComplete() {
        PlayLayer::levelComplete();

        auto levelName   = std::string(m_level->m_levelName);
        auto creatorName = std::string(m_level->m_creatorName);

        sendWebhook(
            "notify-level-complete",
            "Level Complete!",
            fmt::format("Just beat **{}** by **{}**!", levelName, creatorName),
            16766720,
            {
                { "Level", levelName, true },
                { "Creator", creatorName.empty() ? "Unknown" : creatorName, true }
            }
        );
    }
};

class $modify(MyLevelEditorLayer, LevelEditorLayer) {
    bool init(GJGameLevel* level, bool unk) {
        if (!LevelEditorLayer::init(level, unk)) return false;

        auto levelName = std::string(level->m_levelName);

        sendWebhook(
            "notify-editor",
            "Opened the Editor",
            fmt::format("Working on **{}** in the level editor.", levelName.empty() ? "a level" : levelName),
            15105570
        );

        return true;
    }
};
