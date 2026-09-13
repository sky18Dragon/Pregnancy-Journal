#include <cassert>

#include "canvas.h"
#include "text_layout.h"

int main()
{
    uint8_t framebuffer[200U * 80U / 2U] = {};
    Canvas canvas(200U, 80U, framebuffer, sizeof(framebuffer));
    assert(ui_draw_wrapped_text(canvas, 0, 0, 72,
                                "ONE TWO THREE FOUR", 2,
                                GrayLevel::Black, 4) >= 2);
    assert(ui_draw_wrapped_text(canvas, 0, 0, 24, "孕期提示", 2,
                                GrayLevel::Black, 2) == 2);
    assert(ui_draw_wrapped_text(canvas, 0, 0, 0, "TEXT", 2,
                                GrayLevel::Black, 2) == 0);
    return 0;
}
