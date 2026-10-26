#include <cassert>
#include <string>
#include <vector>

#include "canvas.h"
#include "text_layout.h"
#include "ui_language.h"
#include "ui_tokens.h"

namespace {
std::vector<std::string> lines;
std::vector<int> widths;
void observe(int, int, int width, int, const char *text) {
    lines.emplace_back(text);
    widths.push_back(width);
}

void test_chinese_punctuation(Canvas &canvas) {
    ui_language_set(UiLanguage::ChineseSimplified);
    canvas_set_text_observer(observe);
    // A closing mark just beyond the line width must stay with the preceding
    // character. Verify UTF-8/content preservation as well as geometry.
    for (const char *text : {"记录胎动，查看规律。", "记录（胎动）查看规律。"}) {
        lines.clear();
        widths.clear();
        const int width = font_text_width_pixels("记录胎动", ui_tokens::reading_size(),
                                                 FontWeight::Regular);
        const int count = ui_draw_wrapped_text_pixels(
            canvas, 0, 0, width, text, ui_tokens::reading_size(),
            GrayLevel::Black, 8, 4, FontWeight::Regular);
        assert(count > 1);
        std::string joined;
        for (size_t i = 0; i < lines.size(); ++i) {
            assert(widths[i] <= width);
            assert(lines[i].find("，") != 0);
            assert(lines[i].find("。") != 0);
            assert(lines[i].find("）") != 0);
            assert(lines[i].size() < 3 ||
                   lines[i].substr(lines[i].size() - 3) != "（");
            joined += lines[i];
        }
        assert(joined == text);
    }
    canvas_set_text_observer(nullptr);
    ui_language_set(UiLanguage::English);
}
} // namespace

int main()
{
    uint8_t framebuffer[200U * 80U / 2U] = {};
    Canvas canvas(200U, 80U, framebuffer, sizeof(framebuffer));
    test_chinese_punctuation(canvas);
    assert(ui_draw_wrapped_text(canvas, 0, 0, 72,
                                "ONE TWO THREE FOUR", 2,
                                GrayLevel::Black, 4) >= 2);
    assert(ui_draw_wrapped_text(canvas, 0, 0, 24, "孕期提示", 2,
                                GrayLevel::Black, 2) == 2);
    assert(ui_draw_wrapped_text(canvas, 0, 0, 0, "TEXT", 2,
                                GrayLevel::Black, 2) == 0);
    return 0;
}
