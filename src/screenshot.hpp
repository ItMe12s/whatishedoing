#pragma once

#include <cstdint>
#include <optional>
#include <vector>

class PlayLayer;

std::optional<std::vector<std::uint8_t>> capturePlayLayerScreenshotPng(
    PlayLayer* playLayer
);
