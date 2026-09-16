#pragma once

#include <cstddef>
#include <cstdint>

#include "font.h"

struct ChineseFontGlyph {
  uint32_t codepoint;
  uint32_t bitmap_offset;
  uint8_t width;
  uint8_t height;
  int8_t bearing_x;
  int8_t bearing_y;
  uint8_t advance;
};

struct ChineseFontStrike {
  uint8_t pixel_size;
  uint8_t ascent;
  uint8_t descent;
  const ChineseFontGlyph *glyphs;
  size_t glyph_count;
  const uint8_t *bitmap;
};

// Resolves to the closest committed native-size strike. The renderer copies
// strike pixels exactly and never scales a glyph bitmap at runtime.
const ChineseFontStrike &chinese_font_strike(
    uint8_t pixel_size, FontWeight weight = FontWeight::Medium);
const ChineseFontGlyph *chinese_font_glyph(
    const ChineseFontStrike &strike, uint32_t codepoint);
size_t chinese_font_glyph_count();
