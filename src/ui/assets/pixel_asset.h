#pragma once

#include <cstdint>

#include "canvas.h"

struct PixelAsset {
    uint16_t width;
    uint16_t height;
    const uint8_t *data;
};

// Draws an MSB-first 1-bit asset with transparent unset pixels.
// 绘制高位优先的1位素材，未置位像素保持画布原样。
void pixel_asset_draw(Canvas &canvas,
                      int x,
                      int y,
                      const PixelAsset &asset,
                      uint8_t scale = 1,
                      GrayLevel color = GrayLevel::Black);

// Centers a scaled asset around one canvas coordinate.
// 将缩放后的素材围绕指定画布坐标居中绘制。
void pixel_asset_draw_centered(Canvas &canvas,
                               int center_x,
                               int center_y,
                               const PixelAsset &asset,
                               uint8_t scale = 1,
                               GrayLevel color = GrayLevel::Black);
