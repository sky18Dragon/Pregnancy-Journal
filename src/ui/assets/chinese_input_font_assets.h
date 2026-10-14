#pragma once

#include <cstddef>
#include <cstdint>

#include "chinese_font_assets.h"

// A visible, deterministic marker for valid Unicode codepoints outside the
// bounded input-font coverage. Unsupported text must never disappear silently.
constexpr uint32_t kChineseInputMissingGlyphCodepoint = 0x25A1U;  // □

// Dedicated GB2312 Level-1 strikes for offline text input. These do not
// replace the product UI face. Besides printable ASCII and GB2312 Level-1 Han,
// they contain the product's common Chinese punctuation and missing marker.
const ChineseFontStrike &chinese_input_font_strike(uint8_t pixel_size);
const ChineseFontGlyph *chinese_input_font_glyph(
    const ChineseFontStrike &strike, uint32_t codepoint);
const ChineseFontGlyph *chinese_input_font_resolve_glyph(
    const ChineseFontStrike &strike, uint32_t codepoint);
size_t chinese_input_font_glyph_count();
size_t chinese_input_font_resource_bytes();
