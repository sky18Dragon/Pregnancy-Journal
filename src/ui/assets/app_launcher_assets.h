#pragma once

#include <cstdint>

#include "pixel_asset.h"

enum class AppLauncherAssetId : uint8_t {
    Pet,
    Focus,
    Status,
    Answers,
};

struct AppLauncherStickerAsset {
    PixelAsset gray;
    PixelAsset black;
};

// Returns both grayscale layers for one launcher sticker illustration.
// 返回一张应用选择贴纸的浅灰层与黑色层。
const AppLauncherStickerAsset &app_launcher_sticker_asset(
    AppLauncherAssetId id);
