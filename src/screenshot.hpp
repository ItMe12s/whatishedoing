#pragma once

#include <Geode/utils/function.hpp>
#include <cstdint>
#include <optional>
#include <vector>

class PlayLayer;

struct CapturedScreenshotRgba {
    std::vector<std::uint8_t> rgba;
    int width = 0;
    int height = 0;
};

using ScreenshotPng = std::optional<std::vector<std::uint8_t>>;
using ScreenshotCallback = geode::Function<void(ScreenshotPng)>;
using ScreenshotValidity = geode::Function<bool()>;

void capturePlayLayerScreenshotAfterDelay(
    PlayLayer* playLayer, ScreenshotValidity isStillValid, ScreenshotCallback onMainThread
);
