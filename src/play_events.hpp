#pragma once

#include <Geode/utils/function.hpp>
#include <chrono>
#include <cstdint>
#include <string>

class PlayLayer;

namespace play_events {

    void syncPlayMode(PlayLayer* layer);
    bool matchesLevelSession(
        int levelID, std::string const& levelName, std::chrono::steady_clock::time_point attemptStart
    );
    void queueStartposSegmentStart(PlayLayer* layer);
    void reopenLevelSessionIfNeeded(PlayLayer* layer);
    void sendDeathWebhookIfNeeded(
        PlayLayer* layer, int currentPercent, int bestBefore, geode::Function<bool()> captureStillValid
    );
    void sendNewBestWebhookIfNeeded(
        PlayLayer* layer, int percentAtDeath, int bestBeforeDeath,
        geode::Function<bool()> captureStillValid
    );
    void clearCompletedLevelExit(PlayLayer* layer);
    void queueCompletedLevelExit(PlayLayer* layer, std::int64_t elapsedMilliseconds);
    void sendCompletedLevelExitIfQueued(PlayLayer* layer);
    bool consumeSentCompletedLevelExit(PlayLayer* layer);
    void markCurrentBestHandled(PlayLayer* layer, int percentAtDeath = -1, int bestBeforeDeath = -1);

} // namespace play_events
