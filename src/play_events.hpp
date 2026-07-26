#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
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
        PlayLayer* layer, int currentPercent, int bestBefore, std::function<bool()> captureStillValid
    );
    void sendNewBestWebhookIfNeeded(
        PlayLayer* layer, int percentAtDeath, int bestBeforeDeath,
        std::function<bool()> captureStillValid
    );
    void clearCompletedLevelExit(PlayLayer* layer);
    void queueCompletedLevelExit(PlayLayer* layer, std::int64_t elapsedMilliseconds);
    void sendCompletedLevelExitIfQueued(PlayLayer* layer);
    bool consumeSentCompletedLevelExit(PlayLayer* layer);
    void markCurrentBestHandled(PlayLayer* layer, int percentAtDeath = -1, int bestBeforeDeath = -1);

} // namespace play_events
