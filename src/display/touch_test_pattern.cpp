#include "touch_test_pattern.h"

#include "canvas.h"
#include "touch_test_targets.h"

namespace {

constexpr int kTargetHalfSize = 24;

int text_width(const char *text, int scale)
{
    int length = 0;
    while (text[length] != '\0') {
        ++length;
    }
    return length * 6 * scale;
}

void draw_target(Canvas &canvas, const TouchTestTarget &target)
{
    canvas.draw_rect(target.x - kTargetHalfSize,
                     target.y - kTargetHalfSize,
                     kTargetHalfSize * 2 + 1,
                     kTargetHalfSize * 2 + 1,
                     GrayLevel::Black);
    canvas.draw_line(target.x - kTargetHalfSize - 12,
                     target.y,
                     target.x + kTargetHalfSize + 12,
                     target.y,
                     GrayLevel::Black);
    canvas.draw_line(target.x,
                     target.y - kTargetHalfSize - 12,
                     target.x,
                     target.y + kTargetHalfSize + 12,
                     GrayLevel::Black);

    const int label_x =
        target.x - text_width(target.name, 1) / 2;
    canvas.draw_text(label_x,
                     target.y + kTargetHalfSize + 16,
                     target.name,
                     1,
                     GrayLevel::Black);
}

}  // namespace

void touch_test_pattern_render(Canvas &canvas)
{
    canvas.clear(GrayLevel::White);
    canvas.draw_rect(10,
                     10,
                     canvas.width() - 20,
                     canvas.height() - 20,
                     GrayLevel::Black);

    constexpr char kTitle[] = "TOUCH 5 POINT TEST";
    constexpr char kInstruction[] = "TAP EACH TARGET";
    canvas.draw_text((canvas.width() - text_width(kTitle, 3)) / 2,
                     22,
                     kTitle,
                     3,
                     GrayLevel::Black);
    canvas.draw_text((canvas.width() - text_width(kInstruction, 2)) / 2,
                     52,
                     kInstruction,
                     2,
                     GrayLevel::Black);

    for (const TouchTestTarget &target : kTouchTestTargets) {
        draw_target(canvas, target);
    }
}
