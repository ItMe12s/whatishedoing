#pragma once

#include <optional>

enum class RunMode {
    Normal,
    Practice,
    Startpos,
};

RunMode deriveRunMode(bool practice, bool startpos) noexcept;

namespace play_policy {

    bool shouldCaptureStartposSegment(RunMode mode) noexcept;
    bool segmentMeetsThreshold(int startPercent, int endPercent, int minimumProgress) noexcept;

    struct DeathPolicy {
        bool active = false;
        bool alreadyNotified = false;
        bool progressLegal = true;
        bool platformer = false;
        RunMode mode = RunMode::Normal;
        int currentPercent = 0;
        int bestBefore = 0;
        bool notifyNewBest = false;
        int startPercent = 0;
        int normalMinimumPercent = 0;
        int startposMinimumProgress = 0;
    };

    bool shouldNotifyDeath(DeathPolicy const& policy) noexcept;

    int effectiveBest(int storedBest, int percentAtDeath = -1, int bestBeforeDeath = -1) noexcept;

    struct NewBestPolicy {
        bool enabled = false;
        bool active = false;
        bool sameLevel = false;
        bool progressLegal = true;
        RunMode mode = RunMode::Normal;
        int startPercent = 0;
        int bestNotifiedPercent = 0;
        int storedBest = 0;
        int percentAtDeath = -1;
        int bestBeforeDeath = -1;
        int minimumPercent = 0;
        bool redacted = false;
        bool suppressRedacted = false;
    };

    std::optional<int> newBestToNotify(NewBestPolicy const& policy) noexcept;

} // namespace play_policy
