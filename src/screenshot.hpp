#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class PlayLayer;

struct CapturedScreenshotRgba {
    std::vector<std::uint8_t> rgba;
    int width = 0;
    int height = 0;
};

std::optional<CapturedScreenshotRgba> capturePlayLayerScreenshotRgba(
    PlayLayer* playLayer
);

void spawnScreenshotEncodeToPngThen(
    CapturedScreenshotRgba captured,
    int scalePercentClamped,
    std::function<void(std::optional<std::vector<std::uint8_t>> png)> onMainThread
);
