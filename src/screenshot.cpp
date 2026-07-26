#include "screenshot.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/cocos.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/general.hpp>
#include <Geode/utils/random.hpp>
#include <Geode/utils/string.hpp>
#include <algorithm>
#include <cmath>
#include <cocos2d.h>
#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

using namespace geode::prelude;
using namespace cocos2d;

namespace {

    float lanczos2(float x) {
        constexpr float radius = 2.f;
        constexpr float pi = 3.14159265358979323846f; // Not using M_PI today.
        x = std::abs(x);
        if (x == 0) {
            return 1.f;
        }
        if (x >= radius) {
            return 0.f;
        }
        float const pix = pi * x;
        return radius * std::sin(pix) * std::sin(pix / radius) / (pix * pix);
    }

    struct LanczosKernel {
        int first = 0;
        std::vector<float> weights;
    };

    std::vector<LanczosKernel> makeLanczosKernels(int sourceSize, int targetSize) {
        float const scale = static_cast<float>(targetSize) / static_cast<float>(sourceSize);
        float const support = 2.f / scale;
        std::vector<LanczosKernel> kernels(static_cast<size_t>(targetSize));
        for (int target = 0; target < targetSize; ++target) {
            float const center = (static_cast<float>(target) + .5f) / scale - .5f;
            int const first = std::max(0, static_cast<int>(std::ceil(center - support)));
            int const last = std::min(sourceSize - 1, static_cast<int>(std::floor(center + support)));
            auto& kernel = kernels[static_cast<size_t>(target)];
            kernel.first = first;
            kernel.weights.reserve(static_cast<size_t>(last - first + 1));
            float total = 0.f;
            for (int source = first; source <= last; ++source) {
                float const weight = lanczos2((static_cast<float>(source) - center) * scale);
                kernel.weights.push_back(weight);
                total += weight;
            }
            for (float& weight : kernel.weights) {
                weight /= total;
            }
        }
        return kernels;
    }

    std::vector<GLubyte> downscaleRgbaLanczos(GLubyte const* src, int sw, int sh, int dw, int dh) {
        auto const kernelsX = makeLanczosKernels(sw, dw);
        auto const kernelsY = makeLanczosKernels(sh, dh);
        std::vector<float> horizontal(static_cast<size_t>(dw) * static_cast<size_t>(sh) * 3);
        for (int y = 0; y < sh; ++y) {
            for (int x = 0; x < dw; ++x) {
                auto const& kernel = kernelsX[static_cast<size_t>(x)];
                float r = 0.f;
                float g = 0.f;
                float b = 0.f;
                for (size_t tap = 0; tap < kernel.weights.size(); ++tap) {
                    size_t const source = (static_cast<size_t>(y) * static_cast<size_t>(sw) +
                                           static_cast<size_t>(kernel.first) + tap) *
                        4;
                    float const weight = kernel.weights[tap];
                    r += static_cast<float>(src[source]) * weight;
                    g += static_cast<float>(src[source + 1]) * weight;
                    b += static_cast<float>(src[source + 2]) * weight;
                }
                size_t const target =
                    (static_cast<size_t>(y) * static_cast<size_t>(dw) + static_cast<size_t>(x)) * 3;
                horizontal[target] = r;
                horizontal[target + 1] = g;
                horizontal[target + 2] = b;
            }
        }

        std::vector<GLubyte> out(static_cast<size_t>(dw) * static_cast<size_t>(dh) * 4);
        for (int y = 0; y < dh; ++y) {
            auto const& kernel = kernelsY[static_cast<size_t>(y)];
            for (int x = 0; x < dw; ++x) {
                float r = 0.f;
                float g = 0.f;
                float b = 0.f;
                for (size_t tap = 0; tap < kernel.weights.size(); ++tap) {
                    size_t const source =
                        ((static_cast<size_t>(kernel.first) + tap) * static_cast<size_t>(dw) +
                         static_cast<size_t>(x)) *
                        3;
                    float const weight = kernel.weights[tap];
                    r += horizontal[source] * weight;
                    g += horizontal[source + 1] * weight;
                    b += horizontal[source + 2] * weight;
                }
                size_t const target =
                    (static_cast<size_t>(y) * static_cast<size_t>(dw) + static_cast<size_t>(x)) * 4;
                out[target] =
                    static_cast<GLubyte>(std::clamp(static_cast<int>(std::lround(r)), 0, 255));
                out[target + 1] =
                    static_cast<GLubyte>(std::clamp(static_cast<int>(std::lround(g)), 0, 255));
                out[target + 2] =
                    static_cast<GLubyte>(std::clamp(static_cast<int>(std::lround(b)), 0, 255));
                out[target + 3] = 255;
            }
        }
        return out;
    }

    ScreenshotPng encodeRgbaToPngBytes(
        std::vector<std::uint8_t> flippedRgba, int pixelWidth, int pixelHeight, int pct,
        std::filesystem::path const& tmp
    ) {
        if (pixelWidth <= 0 || pixelHeight <= 0 || flippedRgba.empty()) {
            return std::nullopt;
        }

        GLubyte* encodePixels = reinterpret_cast<GLubyte*>(flippedRgba.data());
        int encodeW = pixelWidth;
        int encodeH = pixelHeight;
        std::vector<GLubyte> scaledRgba;
        if (pct < 100) {
            double const f = static_cast<double>(pct) / 100.0;
            int const dw =
                std::max(1, static_cast<int>(std::floor(static_cast<double>(pixelWidth) * f)));
            int const dh =
                std::max(1, static_cast<int>(std::floor(static_cast<double>(pixelHeight) * f)));
            if (dw < pixelWidth || dh < pixelHeight) {
                scaledRgba = downscaleRgbaLanczos(encodePixels, pixelWidth, pixelHeight, dw, dh);
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

        auto const tmpString = geode::utils::string::pathToString(tmp);
        bool const saved = image.saveToFile(tmpString.c_str(), true);
        image.m_pData = nullptr;
        std::error_code ec;
        if (!saved) {
            std::filesystem::remove(tmp, ec);
            return std::nullopt;
        }

        auto readResult = geode::utils::file::readBinary(tmp);
        std::filesystem::remove(tmp, ec);
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

std::optional<CapturedScreenshotRgba> capturePlayLayerScreenshotRgba(PlayLayer* playLayer) {
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
    glview->m_fScaleX = static_cast<float>(pixelWidth) / size.width / displayFactor;
    glview->m_fScaleY = static_cast<float>(pixelHeight) / size.height / displayFactor;

    auto const aspectRatio = static_cast<float>(pixelWidth) / static_cast<float>(pixelHeight);
    auto const newRes = CCSize{std::round(320.f * aspectRatio), 320.f};

    director->m_obWinSizeInPoints = newRes;
    glview->m_obScreenSize = CCSize{static_cast<float>(pixelWidth), static_cast<float>(pixelHeight)};
    glview->setDesignResolutionSize(newRes.width, newRes.height, kResolutionExactFit);

    rt->beginWithClear(0, 0, 0, 0);
    playLayer->visit();

    auto const bufBytes = static_cast<size_t>(pixelWidth) * static_cast<size_t>(pixelHeight) * 4;
    CapturedScreenshotRgba cap;
    cap.width = pixelWidth;
    cap.height = pixelHeight;
    cap.rgba.resize(bufBytes);
    GLint packAlign = 0;
    glGetIntegerv(GL_PACK_ALIGNMENT, &packAlign);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, pixelWidth, pixelHeight, GL_RGBA, GL_UNSIGNED_BYTE, cap.rgba.data());
    glPixelStorei(GL_PACK_ALIGNMENT, packAlign);

    rt->end();

    glview->m_fScaleX = oldScaleX;
    glview->m_fScaleY = oldScaleY;
    director->m_obWinSizeInPoints = oldResolution;
    glview->m_obScreenSize = oldScreenSize;
    glview->setDesignResolutionSize(oldResolution.width, oldResolution.height, kResolutionExactFit);
    director->setViewport();

    auto const rowBytes = static_cast<size_t>(pixelWidth) * 4;
    for (int i = 0; i < pixelHeight / 2; ++i) {
        auto* top = cap.rgba.data() + static_cast<size_t>(i) * rowBytes;
        auto* bottom = cap.rgba.data() + static_cast<size_t>(pixelHeight - i - 1) * rowBytes;
        std::swap_ranges(top, top + rowBytes, bottom);
    }

    return cap;
}

void spawnScreenshotEncodeToPngThen(
    CapturedScreenshotRgba captured, int scalePct, ScreenshotCallback onMainThread
) {
    auto const tmp = Mod::get()->getTempDir() /
        fmt::format("whatishedoing_cap_{}.png", geode::utils::random::generateUUID());
    auto task = geode::async::runtime().spawnBlocking<ScreenshotPng>(
        [cap = std::move(captured), scalePct, tmp]() mutable {
            return encodeRgbaToPngBytes(std::move(cap.rgba), cap.width, cap.height, scalePct, tmp);
        }
    );
    geode::async::spawn(std::move(task), std::move(onMainThread));
}

namespace {

    void captureScreenshotThen(
        PlayLayer* layer, geode::FunctionRef<bool()> isStillValid, ScreenshotCallback callback
    ) {
        if (!layer || PlayLayer::get() != layer || !isStillValid()) {
            callback(std::nullopt);
            return;
        }
        auto captured = capturePlayLayerScreenshotRgba(layer);
        if (!captured) {
            callback(std::nullopt);
            return;
        }
        spawnScreenshotEncodeToPngThen(
            std::move(*captured),
            static_cast<int>(Mod::get()->getSettingValue<int64_t>("screenshot-scale-percent")),
            std::move(callback)
        );
    }

    struct PendingScreenshotCapture {
        WeakRef<PlayLayer> layer;
        ScreenshotValidity isStillValid;
        ScreenshotCallback callback;

        PendingScreenshotCapture(
            PlayLayer* playLayer, ScreenshotValidity validity, ScreenshotCallback onMainThread
        ) :
            layer(playLayer), isStillValid(std::move(validity)), callback(std::move(onMainThread)) {}

        void capture() {
            auto locked = layer.lock();
            captureScreenshotThen(locked.data(), isStillValid, std::move(callback));
        }

        ~PendingScreenshotCapture() {
            if (callback) {
                callback(std::nullopt);
            }
        }
    };

} // namespace

void capturePlayLayerScreenshotAfterDelay(
    PlayLayer* playLayer, ScreenshotValidity isStillValid, ScreenshotCallback onMainThread
) {
    float const delay = std::clamp(
        static_cast<float>(Mod::get()->getSettingValue<double>("screenshot-delay")), 0.f, 0.5f
    );
    if (delay <= 0.f) {
        captureScreenshotThen(playLayer, isStillValid, std::move(onMainThread));
        return;
    }
    auto* scene = CCDirector::sharedDirector()->getRunningScene();
    if (!scene) {
        onMainThread(std::nullopt);
        return;
    }
    auto pending = std::make_shared<PendingScreenshotCapture>(
        playLayer, std::move(isStillValid), std::move(onMainThread)
    );
    scene->runAction(
        CCSequence::create(
            CCDelayTime::create(delay),
            geode::cocos::CallFuncExt::create([pending] {
                pending->capture();
            }),
            nullptr
        )
    );
}
