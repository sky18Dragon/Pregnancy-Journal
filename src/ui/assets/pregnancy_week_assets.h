#pragma once

#include <cstdint>

#include "pixel_asset.h"

// Returns the normalized 148x148 1-bit ImageGen illustration for weeks 6-40.
// Week values outside that range are clamped to the nearest available asset.
const PixelAsset &pregnancy_week_size_asset(uint8_t week);
