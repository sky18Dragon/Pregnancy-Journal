#include <cassert>
#include <cstdint>
#include <initializer_list>

#include "chinese_input_font_assets.h"

int main()
{
    assert(chinese_input_font_glyph_count() == 3871U);
    assert(chinese_input_font_resource_bytes() > 500000U);
    assert(chinese_input_font_resource_bytes() < 800000U);
    for (uint8_t size : {18U, 24U}) {
        const ChineseFontStrike &strike = chinese_input_font_strike(size);
        assert(strike.pixel_size == size);
        assert(strike.glyph_count == 3871U);
        assert(chinese_input_font_glyph(strike, 'A') != nullptr);
        assert(chinese_input_font_glyph(strike, 0x5B55U) != nullptr);  // 孕
        assert(chinese_input_font_glyph(strike, 0x4E2DU) != nullptr);  // 中
        assert(chinese_input_font_glyph(strike, 0x9F9FU) != nullptr);  // 龟
        for (uint32_t punctuation : {0xFF0CU, 0x3002U, 0xFF01U, 0xFF1FU,
                                     0xFF1BU, 0xFF1AU, 0x3001U, 0x201CU,
                                     0x201DU, 0x2018U, 0x2019U, 0xFF08U,
                                     0xFF09U, 0x300AU, 0x300BU, 0x3010U,
                                     0x3011U, 0x2026U, 0x2014U, 0x00B7U}) {
            assert(chinese_input_font_glyph(strike, punctuation) != nullptr);
        }
        const ChineseFontGlyph *missing = chinese_input_font_glyph(
            strike, kChineseInputMissingGlyphCodepoint);
        assert(missing != nullptr);
        assert(missing->width > 0U && missing->height > 0U);
        assert(chinese_input_font_glyph(strike, 0x1F600U) == nullptr);
        assert(chinese_input_font_resolve_glyph(strike, 0x1F600U) == missing);
        assert(chinese_input_font_resolve_glyph(strike, 0x5B55U) ==
               chinese_input_font_glyph(strike, 0x5B55U));
    }
    assert(chinese_input_font_strike(20U).pixel_size == 18U);
    assert(chinese_input_font_strike(23U).pixel_size == 24U);
}
