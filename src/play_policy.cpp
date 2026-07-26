#include "play_policy.hpp"

#include <algorithm>

RunMode deriveRunMode(bool practice, bool startpos) noexcept {
    if (practice) {
        return RunMode::Practice;
    }
    return startpos ? RunMode::Startpos : RunMode::Normal;
}

namespace play_policy {

    bool shouldCaptureStartposSegment(RunMode mode) noexcept {
        return mode == RunMode::Startpos;
    }

    bool segmentMeetsThreshold(int startPercent, int endPercent, int minimumProgress) noexcept {
        int const progress = endPercent - startPercent;
        return progress >= 0 && progress >= minimumProgress;
    }

    bool shouldNotifyDeath(DeathPolicy const& p) noexcept {
        if (!p.active || p.alreadyNotified || !p.progressLegal || p.mode == RunMode::Practice ||
            p.platformer || p.currentPercent <= 0 || p.currentPercent >= 100) {
            return false;
        }
        if (p.mode == RunMode::Startpos) {
            return segmentMeetsThreshold(p.startPercent, p.currentPercent, p.startposMinimumProgress);
        }
        if (p.currentPercent > p.bestBefore && p.notifyNewBest) {
            return false;
        }
        return p.currentPercent >= p.normalMinimumPercent;
    }

    int effectiveBest(int storedBest, int percentAtDeath, int bestBeforeDeath) noexcept {
        if (bestBeforeDeath >= 0 && percentAtDeath > bestBeforeDeath) {
            return std::max(storedBest, percentAtDeath);
        }
        return storedBest;
    }

    std::optional<int> newBestToNotify(NewBestPolicy const& p) noexcept {
        if (!p.enabled || !p.active || !p.sameLevel || !p.progressLegal ||
            p.mode == RunMode::Practice || (p.mode == RunMode::Startpos && p.startPercent > 0) ||
            (p.redacted && p.suppressRedacted)) {
            return std::nullopt;
        }
        int const best = effectiveBest(p.storedBest, p.percentAtDeath, p.bestBeforeDeath);
        if (best <= p.bestNotifiedPercent || best <= p.startPercent || best >= 100 ||
            best < p.minimumPercent) {
            return std::nullopt;
        }
        return best;
    }

} // namespace play_policy
