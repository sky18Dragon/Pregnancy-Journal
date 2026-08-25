#pragma once

#include <cstdint>

#include "pixel_asset.h"

constexpr uint8_t kOnboardingAssetCount = 8U;

// Returns one approved full-screen tutorial bitmap.
// 返回一张已确认的全屏教程位图。
const PixelAsset &onboarding_asset(uint8_t page_index);
