#include "screenshot.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/general.hpp>
#include <Geode/utils/string.hpp>

#include <cocos2d.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <memory>
#include <vector>

using namespace geode::prelude;
using namespace cocos2d;

namespace {

GLubyte clampU8(float x) {
    if (x <= 0.f) {
        return 0;
    }
    if (x >= 255.f) {
        return 255;
    }
    return static_cast<GLubyte>(x + 0.5f);
}

std::vector<GLubyte> downscaleRgbaBilinear(
    GLubyte const* src,
    int sw,
    int sh,
    int dw,
    int dh
) {
    std::vector<GLubyte> out(
        static_cast<size_t>(dw) * static_cast<size_t>(dh) * 4
    );
    float const sx = static_cast<float>(sw) / static_cast<float>(dw);
    float const sy = static_cast<float>(sh) / static_cast<float>(dh);
    for (int y = 0; y < dh; ++y) {
        float const fy =
            (static_cast<float>(y) + 0.5f) * sy - 0.5f;
        int const y0 = std::clamp(static_cast<int>(std::floor(fy)), 0, sh - 1);
        int const y1 = std::clamp(y0 + 1, 0, sh - 1);
        float const wy = fy - static_cast<float>(y0);
        for (int x = 0; x < dw; ++x) {
            float const fx =
                (static_cast<float>(x) + 0.5f) * sx - 0.5f;
            int const x0 = std::clamp(static_cast<int>(std::floor(fx)), 0, sw - 1);
            int const x1 = std::clamp(x0 + 1, 0, sw - 1);
            float const wx = fx - static_cast<float>(x0);
            auto sample = [&](int xi, int yi) -> GLubyte const* {
                return &src[static_cast<size_t>(yi * sw + xi) * 4];
            };
            GLubyte const* c00 = sample(x0, y0);
            GLubyte const* c10 = sample(x1, y0);
            GLubyte const* c01 = sample(x0, y1);
            GLubyte const* c11 = sample(x1, y1);
            for (int c = 0; c < 4; ++c) {
                float const v =
                    (1.f - wx) * (1.f - wy) * static_cast<float>(c00[c]) +
                    wx * (1.f - wy) * static_cast<float>(c10[c]) +
                    (1.f - wx) * wy * static_cast<float>(c01[c]) +
                    wx * wy * static_cast<float>(c11[c]);
                out[static_cast<size_t>(y * dw + x) * 4 + c] = clampU8(v);
            }
        }
    }
    return out;
}

} // namespace

std::optional<std::vector<std::uint8_t>> capturePlayLayerScreenshotPng(
    PlayLayer* playLayer
) {
    if (!playLayer) {
        return std::nullopt;
    }

    auto* director = CCDirector::sharedDirector();
    if (!director) {
        return std::nullopt;
    }
    auto* glview = director->getOpenGLView();
    if (!glview) {
        return std::nullopt;
    }

    auto const size = director->getWinSize();
    int const logicalWidth = static_cast<int>(size.width);
    int const logicalHeight = static_cast<int>(size.height);
    if (logicalWidth <= 0 || logicalHeight <= 0) {
        return std::nullopt;
    }

    auto* rt = CCRenderTexture::create(logicalWidth, logicalHeight);
    if (!rt) {
        return std::nullopt;
    }

    auto const texSize = rt->getSprite()->getTexture()->getContentSizeInPixels();
    int const pixelWidth = static_cast<int>(texSize.width);
    int const pixelHeight = static_cast<int>(texSize.height);
    if (pixelWidth <= 0 || pixelHeight <= 0) {
        return std::nullopt;
    }

    auto const oldScaleX = glview->m_fScaleX;
    auto const oldScaleY = glview->m_fScaleY;
    auto const oldResolution = glview->getDesignResolutionSize();
    auto const oldScreenSize = glview->m_obScreenSize;

    auto const displayFactor = geode::utils::getDisplayFactor();
    glview->m_fScaleX =
        static_cast<float>(pixelWidth) / size.width / displayFactor;
    glview->m_fScaleY =
        static_cast<float>(pixelHeight) / size.height / displayFactor;

    auto const aspectRatio =
        static_cast<float>(pixelWidth) / static_cast<float>(pixelHeight);
    auto const newRes =
        CCSize{std::round(320.f * aspectRatio), 320.f};

    director->m_obWinSizeInPoints = newRes;
    glview->m_obScreenSize =
        CCSize{static_cast<float>(pixelWidth), static_cast<float>(pixelHeight)};
    glview->setDesignResolutionSize(
        newRes.width,
        newRes.height,
        kResolutionExactFit
    );

    rt->beginWithClear(0, 0, 0, 0);
    playLayer->visit();

    auto rawData =
        std::vector<GLubyte>(static_cast<size_t>(pixelWidth * pixelHeight * 4));
    GLint packAlign = 0;
    glGetIntegerv(GL_PACK_ALIGNMENT, &packAlign);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(
        0,
        0,
        pixelWidth,
        pixelHeight,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        rawData.data()
    );
    glPixelStorei(GL_PACK_ALIGNMENT, packAlign);

    rt->end();

    glview->m_fScaleX = oldScaleX;
    glview->m_fScaleY = oldScaleY;
    director->m_obWinSizeInPoints = oldResolution;
    glview->m_obScreenSize = oldScreenSize;
    glview->setDesignResolutionSize(
        oldResolution.width,
        oldResolution.height,
        kResolutionExactFit
    );
    director->setViewport();

    auto const bufBytes =
        static_cast<size_t>(pixelWidth) * static_cast<size_t>(pixelHeight) * 4;
    auto flipped = std::make_unique<GLubyte[]>(bufBytes);
    for (int i = 0; i < pixelHeight; ++i) {
        std::memcpy(
            &flipped[static_cast<size_t>(i * pixelWidth * 4)],
            &rawData[static_cast<size_t>((pixelHeight - i - 1) * pixelWidth * 4)],
            static_cast<size_t>(pixelWidth * 4)
        );
    }

    int const pct = std::clamp(
        static_cast<int>(
            Mod::get()->getSettingValue<int64_t>("screenshot-scale-percent")
        ),
        25,
        100
    );

    std::vector<GLubyte> scaledRgba;
    GLubyte* encodePixels = flipped.get();
    int encodeW = pixelWidth;
    int encodeH = pixelHeight;
    if (pct < 100) {
        double const f = static_cast<double>(pct) / 100.0;
        int const dw = std::max(
            1,
            static_cast<int>(
                std::floor(static_cast<double>(pixelWidth) * f)
            )
        );
        int const dh = std::max(
            1,
            static_cast<int>(
                std::floor(static_cast<double>(pixelHeight) * f)
            )
        );
        if (dw < pixelWidth || dh < pixelHeight) {
            scaledRgba = downscaleRgbaBilinear(
                flipped.get(),
                pixelWidth,
                pixelHeight,
                dw,
                dh
            );
            encodePixels = scaledRgba.data();
            encodeW = dw;
            encodeH = dh;
        }
    }

    CCImage image{};
    image.m_nBitsPerComponent = 8;
    image.m_nWidth = encodeW;
    image.m_nHeight = encodeH;
    image.m_bHasAlpha = true;
    image.m_bPreMulti = false;
    image.m_pData = encodePixels;

    auto const tmpPath = Mod::get()->getSaveDir() / "whatishedoing_cap_tmp.png";
    auto const tmpStr = geode::utils::string::pathToString(tmpPath);
    bool const saved = image.saveToFile(tmpStr.c_str(), true);
    image.m_pData = nullptr;
    if (!saved) {
        return std::nullopt;
    }

    auto readResult = geode::utils::file::readBinary(tmpStr);
    std::error_code ec;
    std::filesystem::remove(tmpPath, ec);
    if (!readResult.isOk()) {
        return std::nullopt;
    }
    auto out = readResult.unwrap();
    if (out.empty()) {
        return std::nullopt;
    }
    return out;
}
