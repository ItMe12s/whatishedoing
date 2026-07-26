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

std::optional<CapturedScreenshotRgba> capturePlayLayerScreenshotRgba(PlayLayer* playLayer);

void spawnScreenshotEncodeToPngThen(
    CapturedScreenshotRgba captured, int scalePercentClamped, ScreenshotCallback onMainThread
);

void capturePlayLayerScreenshotAfterDelay(
    PlayLayer* playLayer, ScreenshotValidity isStillValid, ScreenshotCallback onMainThread
);
