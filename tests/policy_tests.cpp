#include "difficulty_face.hpp"
#include "level_filter.hpp"
#include "play_policy.hpp"
#include "retry_policy.hpp"
#include "text_policy.hpp"

#include <iostream>
#include <optional>
#include <string>

namespace {

    int failures = 0;

    void expect(bool condition, char const* name) {
        if (!condition) {
            std::cerr << "FAIL: " << name << '\n';
            ++failures;
        }
    }

    void testRunModes() {
        expect(deriveRunMode(false, false) == RunMode::Normal, "normal mode");
        expect(deriveRunMode(false, true) == RunMode::Startpos, "startpos mode");
        expect(deriveRunMode(true, false) == RunMode::Practice, "practice mode");
        expect(deriveRunMode(true, true) == RunMode::Practice, "practice takes precedence");
        expect(
            play_policy::shouldCaptureStartposSegment(RunMode::Startpos), "capture startpos segment"
        );
        expect(!play_policy::shouldCaptureStartposSegment(RunMode::Practice), "skip practice segment");
    }

    void testDeathPolicy() {
        play_policy::DeathPolicy death{
            .active = true,
            .currentPercent = 40,
            .bestBefore = 50,
            .normalMinimumPercent = 40,
        };
        expect(play_policy::shouldNotifyDeath(death), "death at threshold");
        death.currentPercent = 39;
        expect(!play_policy::shouldNotifyDeath(death), "death below threshold");
        death.currentPercent = 60;
        death.notifyNewBest = true;
        expect(!play_policy::shouldNotifyDeath(death), "new best owns normal death");
        death.notifyNewBest = false;
        expect(play_policy::shouldNotifyDeath(death), "death owns new best when disabled");
        death.platformer = true;
        expect(!play_policy::shouldNotifyDeath(death), "platformer death excluded");
        death.platformer = false;
        death.progressLegal = false;
        expect(!play_policy::shouldNotifyDeath(death), "cheat death excluded");

        death = {
            .active = true,
            .mode = RunMode::Startpos,
            .currentPercent = 70,
            .startPercent = 50,
            .startposMinimumProgress = 20,
        };
        expect(play_policy::shouldNotifyDeath(death), "startpos death at segment threshold");
        death.currentPercent = 69;
        expect(!play_policy::shouldNotifyDeath(death), "startpos death below segment threshold");
        death.currentPercent = 49;
        death.startposMinimumProgress = 0;
        expect(!play_policy::shouldNotifyDeath(death), "negative startpos progress excluded");
        death.mode = RunMode::Practice;
        death.currentPercent = 70;
        expect(!play_policy::shouldNotifyDeath(death), "practice death excluded");
    }

    void testNewBestPolicy() {
        play_policy::NewBestPolicy best{
            .enabled = true,
            .active = true,
            .sameLevel = true,
            .bestNotifiedPercent = 40,
            .storedBest = 42,
            .minimumPercent = 20,
        };
        expect(play_policy::newBestToNotify(best) == std::optional<int>{42}, "stored new best");
        best.minimumPercent = 43;
        expect(!play_policy::newBestToNotify(best), "new best below minimum");
        best.minimumPercent = 20;
        best.bestNotifiedPercent = 42;
        expect(!play_policy::newBestToNotify(best), "new best already notified");
        best.bestNotifiedPercent = 40;
        best.storedBest = 40;
        best.percentAtDeath = 45;
        best.bestBeforeDeath = 40;
        expect(
            play_policy::newBestToNotify(best) == std::optional<int>{45}, "death snapshot new best"
        );
        best.mode = RunMode::Startpos;
        best.startPercent = 20;
        expect(!play_policy::newBestToNotify(best), "startpos new best excluded");
        best.mode = RunMode::Practice;
        best.startPercent = 0;
        expect(!play_policy::newBestToNotify(best), "practice new best excluded");
        best.mode = RunMode::Normal;
        best.progressLegal = false;
        expect(!play_policy::newBestToNotify(best), "cheat new best excluded");
        best.progressLegal = true;
        best.redacted = true;
        best.suppressRedacted = true;
        expect(!play_policy::newBestToNotify(best), "suppressed private new best");
        best.redacted = false;
        best.suppressRedacted = false;
        best.storedBest = 100;
        best.percentAtDeath = -1;
        best.bestBeforeDeath = -1;
        expect(!play_policy::newBestToNotify(best), "completion is not new best");
    }

    void testLevelFilter() {
        auto const ids = level_filter::parseLevelIds("3 1,2,2 Stereo Madness,0");
        expect(ids == level_filter::LevelIds{0, 1, 2, 3}, "sorted unique level IDs");
        expect(level_filter::parseLevelIds("").empty(), "empty level ID list");
        expect(
            level_filter::parseLevelIds("6\t5\n4\r3\f2\v1,0,6") ==
                level_filter::LevelIds{0, 1, 2, 3, 4, 5, 6},
            "all C-locale separators and duplicates"
        );
        expect(level_filter::parseLevelIds("4x").contains(4), "legacy numeric prefix parsing");
        expect(!level_filter::parseLevelIds("+4").contains(4), "leading plus rejected");
        expect(level_filter::parseLevelIds("x4").empty(), "nonnumeric prefix rejected");
        expect(level_filter::parseLevelIds("-4x").contains(-4), "negative numeric prefix parsing");
        expect(
            level_filter::parseLevelIds("999999999999999999999999").empty(),
            "overflowing level ID rejected"
        );
        expect(
            level_filter::parseMode("Anything") == level_filter::Mode::All,
            "unknown filter mode is All"
        );
        expect(!level_filter::shouldRedact(2, level_filter::Mode::All, ids), "All does not redact");
        expect(
            level_filter::shouldRedact(2, level_filter::Mode::Blacklist, ids),
            "blacklist member redacted"
        );
        expect(
            !level_filter::shouldRedact(9, level_filter::Mode::Blacklist, ids),
            "blacklist nonmember visible"
        );
        expect(
            !level_filter::shouldRedact(2, level_filter::Mode::Whitelist, ids),
            "whitelist member visible"
        );
        expect(
            level_filter::shouldRedact(9, level_filter::Mode::Whitelist, ids),
            "whitelist nonmember redacted"
        );
        expect(
            level_filter::shouldRedact(-1, level_filter::Mode::Whitelist, ids),
            "unknown ID redacted by whitelist"
        );
    }

    void testTextPolicy() {
        expect(text_policy::formatDuration(0) == "0 seconds", "zero duration");
        expect(text_policy::formatDuration(61) == "1 minute and 1 second", "minute duration");
        expect(text_policy::formatDuration(60) == "1 minute", "exact minute duration");
        expect(text_policy::formatDuration(3600) == "1 hour", "exact hour duration");
        expect(text_policy::formatDuration(3661) == "1 hour, 1 minute and 1 second", "hour duration");
        expect(text_policy::formatDurationMs(-1) == "0 seconds", "negative milliseconds clamped");
        expect(text_policy::formatDurationMs(1) == "0.00 seconds", "fractional milliseconds duration");
        expect(text_policy::formatDurationMs(999) == "1.00 seconds", "rounded subsecond duration");
        expect(
            text_policy::formatDurationMs(1000) == "1 second", "whole second milliseconds duration"
        );

        expect(play_policy::segmentMeetsThreshold(80, 100, 20), "startpos completion at threshold");
        expect(
            !play_policy::segmentMeetsThreshold(81, 100, 20), "startpos completion below threshold"
        );
    }

    void testDifficultyFace() {
        expect(getLevelRating(0, 0, 0) == 0, "unrated");
        expect(getLevelRating(0, 0, 5) == 1, "rated");
        expect(getLevelRating(1, 0, 5) == 2, "featured");
        expect(getLevelRating(0, 1, 10) == 3, "epic");
        expect(getLevelRating(0, 2, 10) == 4, "legendary");
        expect(getLevelRating(0, 3, 10) == 5, "mythic");
        expect(getLevelRating(0, 9, 0) == 0, "unknown isEpic falls back to unrated");
        expect(getDifficultyFace(-1, 0, 0) == std::optional<std::string>{"unrated"}, "unrated face");
        expect(
            getDifficultyFace(0, 0, 5, 2) == std::optional<std::string>{"hard-featured"},
            "hard featured face"
        );
        expect(
            getDifficultyFace(6, 6, 10, 5) == std::optional<std::string>{"demon-extreme-mythic"},
            "extreme mythic face"
        );
        expect(
            getDifficultyFace(6, 0, 10, 1) == std::optional<std::string>{"demon-hard"},
            "plain demon is demon-hard"
        );
        expect(
            getDifficultyFace(0, 0, 1, 3) == std::optional<std::string>{"auto-epic"},
            "auto epic face"
        );
        expect(
            getDifficultyFace(7, 0, 0, 4) == std::optional<std::string>{"unrated-legendary"},
            "zero stars falls back to unrated base"
        );
        expect(!getDifficultyFace(6, 99, 10, 1), "unknown demon difficulty is no face");
        expect(!getDifficultyFace(99, 0, 5, 1), "unknown difficulty is no face");

        expect(footerWithLevelId("", true, 123) == "Level ID: 123", "id only footer");
        expect(footerWithLevelId("", false, 123).empty(), "private footer empty");
        expect(
            footerWithLevelId("1 minute", true, 123) == "1 minute \u2022 Level ID: 123",
            "combined footer"
        );
        expect(footerWithLevelId("1 minute", false, 123) == "1 minute", "elapsed only footer");
    }

    void testRetryPolicy() {
        expect(
            retry_policy::delayForFailure(500, std::nullopt, 0, 3) == std::optional<int>{1},
            "first retry delay"
        );
        expect(
            retry_policy::delayForFailure(500, std::nullopt, 2, 3) == std::optional<int>{4},
            "exponential retry delay"
        );
        expect(!retry_policy::delayForFailure(500, std::nullopt, 3, 3), "retry limit");
        expect(
            retry_policy::delayForFailure(429, "7", 0, 3) == std::optional<int>{7},
            "Retry-After honored"
        );
        expect(
            retry_policy::delayForFailure(429, "99", 0, 3, 2) == std::optional<int>{2},
            "Retry-After blocking cap"
        );
        expect(
            retry_policy::delayForFailure(429, "invalid", 0, 3) == std::optional<int>{2},
            "invalid Retry-After fallback"
        );
        expect(
            retry_policy::delayForFailure(429, "7 seconds", 0, 3) == std::optional<int>{7},
            "legacy Retry-After numeric prefix"
        );
    }

} // namespace

int main() {
    testRunModes();
    testDeathPolicy();
    testNewBestPolicy();
    testLevelFilter();
    testTextPolicy();
    testDifficultyFace();
    testRetryPolicy();
    if (failures == 0) {
        std::cout << "All policy tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
