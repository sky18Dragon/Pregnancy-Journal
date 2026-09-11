#pragma once

#include <cstdint>

struct FontGlyph {
    uint8_t columns[5];
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

// Returns the CJK pixel scale used to match the 5x7 Latin line height.
// 返回与5x7拉丁字体行高匹配的中文字形像素倍率。
uint8_t font_cjk_scale(uint8_t latin_scale);

// Measures UTF-8 text using the same mixed ASCII/CJK advances as Canvas.
// 使用与Canvas一致的ASCII/中文混排步进计算UTF-8文字宽度。
int font_text_width(const char *text, uint8_t scale);
