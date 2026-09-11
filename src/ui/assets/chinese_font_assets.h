#pragma once

#include <cstddef>
#include <cstdint>

constexpr uint8_t kChineseFontWidth = 12U;
constexpr uint8_t kChineseFontHeight = 12U;
constexpr uint8_t kChineseFontSpacing = 1U;
// Keep a higher-resolution source bitmap so large CJK text can be resampled
// instead of enlarging a coarse 12x12 bitmap into square blocks.
constexpr uint8_t kChineseFontSourceWidth = 24U;
constexpr uint8_t kChineseFontSourceHeight = 24U;

struct ChineseFontGlyph {
    uint32_t codepoint;
    uint32_t rows[kChineseFontSourceHeight];
};

const ChineseFontGlyph *chinese_font_glyph(uint32_t codepoint);
size_t chinese_font_glyph_count();
