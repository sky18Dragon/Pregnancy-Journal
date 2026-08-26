#pragma once

#include <cstdint>

#include "pixel_asset.h"

constexpr uint8_t kOnboardingAssetCount = 6U;

// Returns one approved full-screen tutorial bitmap.
// 返回一张已确认的全屏教程位图。
const PixelAsset &onboarding_asset(uint8_t page_index);

// Returns the hand-drawn LOVE callout arrow.
// 返回LOVE说明使用的手绘箭头。
const PixelAsset &onboarding_love_arrow_asset();
