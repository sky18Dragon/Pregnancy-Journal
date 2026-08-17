#include "display_test_pattern.h"

#include "canvas.h"

namespace {

constexpr int kOuterMargin = 12;
constexpr int kMarkerSize = 72;
constexpr int kLabelScale = 3;
constexpr int kTitleScale = 6;
constexpr char kTitle[] = "STICKY DISPLAY OK";

// Returns the pixel width of text rendered by the compact 5x7 font.
// 计算5x7点阵字体放大后的文字宽度，供居中布局使用。
int text_width(const char *text, int scale)
{
    int character_count = 0;
    while (text[character_count] != '\0') {
        ++character_count;
    }
    return character_count * 6 * scale;
}

}  // namespace

void display_test_pattern_render(Canvas &canvas)
{
    canvas.clear(GrayLevel::White);

    // The border proves that all four panel edges are addressable.
    // 外框用于确认屏幕四条边都能被正确寻址和显示。
    canvas.draw_rect(kOuterMargin,
                     kOuterMargin,
                     canvas.width() - 2 * kOuterMargin,
                     canvas.height() - 2 * kOuterMargin,
                     GrayLevel::Black);

    // Each corner uses a different shape so a rotation or mirror is obvious.
    // 四个角使用不同图形，画面旋转或镜像时可以直接看出来。
    canvas.fill_rect(30, 30, kMarkerSize, kMarkerSize, GrayLevel::Black);
    canvas.draw_text(42, 52, "TL", kLabelScale, GrayLevel::White);

    canvas.draw_rect(canvas.width() - 30 - kMarkerSize,
                     30,
                     kMarkerSize,
                     kMarkerSize,
                     GrayLevel::Black);
    canvas.draw_line(canvas.width() - 30 - kMarkerSize,
                     30,
                     canvas.width() - 31,
                     30 + kMarkerSize - 1,
                     GrayLevel::Black);
    canvas.draw_text(canvas.width() - 88, 52, "TR", kLabelScale);

    for (int bar = 0; bar < 3; ++bar) {
        canvas.fill_rect(30,
                         canvas.height() - 102 + bar * 24,
                         30 + bar * 22,
                         12,
                         GrayLevel::Black);
    }
    canvas.draw_text(32, canvas.height() - 70, "BL", kLabelScale);

    const int bottom_right_x = canvas.width() - 30 - kMarkerSize;
    const int bottom_right_y = canvas.height() - 30 - kMarkerSize;
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            if (((row + column) & 1) == 0) {
                canvas.fill_rect(bottom_right_x + column * 18,
                                 bottom_right_y + row * 18,
                                 18,
                                 18,
                                 GrayLevel::Black);
            }
        }
    }

    const int title_x =
        (static_cast<int>(canvas.width()) - text_width(kTitle, kTitleScale)) / 2;
    const int title_y =
        (static_cast<int>(canvas.height()) - 7 * kTitleScale) / 2;
    canvas.draw_text(title_x, title_y, kTitle, kTitleScale, GrayLevel::Black);
}
