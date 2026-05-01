#include "screenshot.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/general.hpp>
#include <Geode/utils/string.hpp>

#include <cocos2d.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <memory>
#include <numbers>
#include <vector>

using namespace geode::prelude;
using namespace cocos2d;

// Found this gem on https://www.kevincao.xyz/posts/image-resizing
namespace {

double lanczos2(double x) {
    constexpr double a = 2;
    // M_PI fans vs std::numbers::pi fans vs whatever float const pi = std::numbers::pi_v<float>; is
    // -dank_meme01
    double const pi = std::numbers::pi_v<double>;
    if (x == 0) {
        return 1;
    }
    if (-a <= x && x < a) {
        return a * std::sin(pi * x) * std::sin(pi * x / a)
            / (pi * pi * x * x);
    }
    return 0;
}

struct LanczosKernelEntry {
    int lo = 0;
    int hi = 0;
    std::array<double, 6> weights{};
};

std::vector<GLubyte> downscaleRgbaLanczos(
    GLubyte const* src,
    int sw,
    int sh,
    int dw,
    int dh
) {
    std::vector<GLubyte> out(
        static_cast<size_t>(dw) * static_cast<size_t>(dh) * 4
    );
    double const zoomX = static_cast<double>(dw) / static_cast<double>(sw);
    double const zoomY = static_cast<double>(dh) / static_cast<double>(sh);

    std::vector<LanczosKernelEntry> weightsYs(static_cast<size_t>(dh));
    for (int y = 0; y < dh; ++y) {
        double const srcY = static_cast<double>(y) / zoomY;
        int lo = static_cast<int>(std::ceil(srcY - 3));
        if (lo < 0) {
            lo = 0;
        }
        int hi = static_cast<int>(std::floor(srcY + 3 - 1e-6));
        if (hi > sh - 1) {
            hi = sh - 1;
        }
        weightsYs[static_cast<size_t>(y)].lo = lo;
        weightsYs[static_cast<size_t>(y)].hi = hi;
        for (int y_ = lo; y_ <= hi; ++y_) {
            weightsYs[static_cast<size_t>(y)].weights[static_cast<size_t>(y_ - lo)]
                = lanczos2(static_cast<double>(y_) - srcY);
        }
    }

    std::vector<LanczosKernelEntry> weightsXs(static_cast<size_t>(dw));
    for (int x = 0; x < dw; ++x) {
        double const srcX = static_cast<double>(x) / zoomX;
        int lo = static_cast<int>(std::ceil(srcX - 3));
        if (lo < 0) {
            lo = 0;
        }
        int hi = static_cast<int>(std::floor(srcX + 3 - 1e-6));
        if (hi > sw - 1) {
            hi = sw - 1;
        }
        weightsXs[static_cast<size_t>(x)].lo = lo;
        weightsXs[static_cast<size_t>(x)].hi = hi;
        for (int x_ = lo; x_ <= hi; ++x_) {
            weightsXs[static_cast<size_t>(x)].weights[static_cast<size_t>(x_ - lo)]
                = lanczos2(static_cast<double>(x_) - srcX);
        }
    }

    for (int y = 0; y < dh; ++y) {
        LanczosKernelEntry const& ky = weightsYs[static_cast<size_t>(y)];
        for (int x = 0; x < dw; ++x) {
            LanczosKernelEntry const& kx = weightsXs[static_cast<size_t>(x)];
            size_t const outI =
                static_cast<size_t>(y * dw + x) * 4;
            out[outI + 3] = 255;
            for (int channel = 0; channel < 3; ++channel) {
                double sum = 0;
                double totalWeight = 0;
                for (int y_ = ky.lo; y_ <= ky.hi; ++y_) {
                    double const wy =
                        ky.weights[static_cast<size_t>(y_ - ky.lo)];
                    for (int x_ = kx.lo; x_ <= kx.hi; ++x_) {
                        double const wx =
                            kx.weights[static_cast<size_t>(x_ - kx.lo)];
                        double const weight = wx * wy;
                        sum += static_cast<double>(
                                   src[static_cast<size_t>(y_ * sw + x_) * 4
                                       + channel]
                               )
                            * weight;
                        totalWeight += weight;
                    }
                }
                int const outChannel =
                    totalWeight > 0
                        ? static_cast<int>(sum / totalWeight)
                        : 0;

                out[outI + static_cast<size_t>(channel)] =
                    static_cast<GLubyte>(std::clamp(outChannel, 0, 255));
            }
        }
    }
    return out;
}

std::optional<std::vector<std::uint8_t>> encodeRgbaToPngBytes(
    std::vector<std::uint8_t> flippedRgba,
    int pixelWidth,
    int pixelHeight,
    int pct,
    std::string const& tmpStr
) {
    if (pixelWidth <= 0 || pixelHeight <= 0 || flippedRgba.empty()) {
        return std::nullopt;
    }

    GLubyte* encodePixels =
        reinterpret_cast<GLubyte*>(flippedRgba.data());
    int encodeW = pixelWidth;
    int encodeH = pixelHeight;
    std::vector<GLubyte> scaledRgba;
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
            scaledRgba = downscaleRgbaLanczos(
                encodePixels,
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

    bool const saved = image.saveToFile(tmpStr.c_str(), true);
    image.m_pData = nullptr;
    if (!saved) {
        return std::nullopt;
    }

    auto readResult = geode::utils::file::readBinary(tmpStr);
    std::error_code ec;
    std::filesystem::remove(std::filesystem::path(tmpStr), ec);
    if (!readResult.isOk()) {
        return std::nullopt;
    }
    auto out = readResult.unwrap();
    if (out.empty()) {
        return std::nullopt;
    }
    return out;
}

} // namespace

std::optional<CapturedScreenshotRgba> capturePlayLayerScreenshotRgba(
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
    CapturedScreenshotRgba cap;
    cap.width = pixelWidth;
    cap.height = pixelHeight;
    cap.rgba.resize(bufBytes);
    for (int i = 0; i < pixelHeight; ++i) {
        std::memcpy(
            cap.rgba.data() + static_cast<size_t>(i * pixelWidth * 4),
            rawData.data()
                + static_cast<size_t>((pixelHeight - i - 1) * pixelWidth * 4),
            static_cast<size_t>(pixelWidth * 4)
        );
    }

    return cap;
}

void spawnScreenshotEncodeToPngThen(
    CapturedScreenshotRgba captured,
    int scalePct,
    std::string tmpPathStr,
    std::function<void(std::optional<std::vector<std::uint8_t>> png)> onMainThread
) {
    geode::async::runtime().spawnBlocking<void>([
        cap = std::move(captured),
        scalePct,
        tmp = std::move(tmpPathStr),
        cb = std::move(onMainThread)
    ]() mutable {
        auto png = encodeRgbaToPngBytes(
            std::move(cap.rgba),
            cap.width,
            cap.height,
            scalePct,
            tmp
        );
        geode::queueInMainThread([cb = std::move(cb), png = std::move(png)]() mutable {
            cb(std::move(png));
        });
    });
}
