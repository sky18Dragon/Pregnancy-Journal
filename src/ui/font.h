#pragma once

#include <cstdint>

struct FontGlyph {
  uint8_t columns[5];
};

enum class FontWeight : uint8_t {
  Regular,
  Medium,
};

constexpr uint8_t kFontWidth = 5;
constexpr uint8_t kFontHeight = 7;
constexpr uint8_t kFontSpacing = 1;

// Returns a compact 5x7 glyph. Unsupported characters use the '?' glyph.
// 返回一个5x7点阵字符；当前字体不支持的字符统一显示为问号。
const FontGlyph &font_get_glyph(char character);

// Decodes one UTF-8 code point and advances text to the next character.
// 解码一个UTF-8码点，并把指针移动到下一个字符。
bool font_decode_utf8(const char *&text, uint32_t &codepoint);

// Maps the legacy integer scale to a unified UI text pixel height.
// 1 -> 16 px (secondary caption), 2 -> 18 px (body/operation text, the
// minimum for anything the user must read to act), 3 and above keep the 7 px
// rhythm (21, 28, ...). 12 px is not produced: it is not legible on the panel.
uint8_t font_ui_pixel_size(uint8_t scale);

// Returns the tracking used by the unified UI raster face.
uint8_t font_ui_spacing(uint8_t scale);

// Returns the tracking used when a page requests an exact pixel height.
uint8_t font_ui_spacing_pixels(uint8_t pixel_size);

// Returns the native bitmap strike used for a requested size.
uint8_t font_resolved_pixel_size(uint8_t pixel_size,
                                 FontWeight weight = FontWeight::Medium);

// Measures UTF-8 text using the same mixed ASCII/CJK advances as Canvas.
// 使用与Canvas一致的ASCII/中文混排步进计算UTF-8文字宽度。
int font_text_width(const char *text, uint8_t scale,
                    FontWeight weight = FontWeight::Medium);

// Measures UTF-8 text at an exact pixel height. This is intended for screens
// whose typography needs to match a fixed-resolution layout precisely.
int font_text_width_pixels(const char *text, uint8_t pixel_size,
                           FontWeight weight = FontWeight::Medium);
