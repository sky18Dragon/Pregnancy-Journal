#include "pixel_asset.h"

#include <cstddef>

void pixel_asset_draw(Canvas &canvas,
                      int x,
                      int y,
                      const PixelAsset &asset,
                      uint8_t scale,
                      GrayLevel color)
{
    if (asset.data == nullptr || asset.width == 0U || asset.height == 0U ||
        scale == 0U) {
        return;
    }

    // Each source row is padded to a whole byte and read from bit 7 to bit 0.
    // 每行素材补齐到完整字节，并按第7位到第0位的顺序读取。
    const size_t row_stride = (static_cast<size_t>(asset.width) + 7U) / 8U;
    for (uint16_t source_y = 0; source_y < asset.height; ++source_y) {
        for (uint16_t source_x = 0; source_x < asset.width; ++source_x) {
            const size_t index = static_cast<size_t>(source_y) * row_stride +
                                 source_x / 8U;
            const uint8_t mask = static_cast<uint8_t>(1U << (7U - source_x % 8U));
            if ((asset.data[index] & mask) == 0U) {
                continue;
            }
            canvas.fill_rect(x + source_x * scale,
                             y + source_y * scale,
                             scale,
                             scale,
                             color);
        }
    }
}

void pixel_asset_draw_centered(Canvas &canvas,
                               int center_x,
                               int center_y,
                               const PixelAsset &asset,
                               uint8_t scale,
                               GrayLevel color)
{
    const int scaled_width = static_cast<int>(asset.width) * scale;
    const int scaled_height = static_cast<int>(asset.height) * scale;
    pixel_asset_draw(canvas,
                     center_x - scaled_width / 2,
                     center_y - scaled_height / 2,
                     asset,
                     scale,
                     color);
}
