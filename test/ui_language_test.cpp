#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

#include "canvas.h"
#include "chinese_font_assets.h"
#include "ui_language.h"

int main()
{
    ui_language_set(UiLanguage::English);
    assert(std::strcmp(ui_text("BACK"), "BACK") == 0);
    assert(ui_text_width("BACK", 2) == 46);

    ui_language_set(UiLanguage::ChineseSimplified);
    assert(std::strcmp(ui_text("BACK"), "返回") == 0);
    assert(std::strcmp(ui_text("UNMAPPED"), "UNMAPPED") == 0);
    assert(ui_text_width("BACK", 2) == 25);
    assert(chinese_font_glyph_count() > 500U);

    std::vector<uint8_t> pixels((120U * 40U + 3U) / 4U, 0xFFU);
    Canvas canvas(120, 40, pixels.data(), pixels.size());
    canvas.draw_text(4, 4, "BACK", 2, GrayLevel::Black);
    size_t black_pixels = 0U;
    for (int y = 0; y < 40; ++y) {
        for (int x = 0; x < 120; ++x) {
            black_pixels += canvas.pixel_at(x, y) == GrayLevel::Black ? 1U : 0U;
        }
    }
    assert(black_pixels > 20U);
    return 0;
}
