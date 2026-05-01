#include "screenshot.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/general.hpp>
#include <Geode/utils/string.hpp>

#include <cocos2d.h>

#include <cstring>
#include <filesystem>
#include <memory>

using namespace geode::prelude;
using namespace cocos2d;

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

    CCImage image{};
    image.m_nBitsPerComponent = 8;
    image.m_nWidth = pixelWidth;
    image.m_nHeight = pixelHeight;
    image.m_bHasAlpha = true;
    image.m_bPreMulti = false;
    image.m_pData = flipped.get();

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
