#include <cassert>
#include <cstdint>
#include <vector>

#include "battery_status_overlay_theme.h"
#include "canvas.h"

namespace {

constexpr uint16_t kWidth = 120U;
constexpr uint16_t kHeight = 60U;
constexpr int kAreaX = 16;
constexpr int kAreaY = 4;
constexpr int kAreaWidth = 98;
constexpr int kAreaHeight = 28;

void assert_theme(const Canvas &canvas,
                  GrayLevel background,
                  GrayLevel foreground)
{
    const BatteryStatusOverlayTheme theme =
        battery_status_overlay_theme(
            canvas, kAreaX, kAreaY, kAreaWidth, kAreaHeight);
    assert(theme.background == background);
    assert(theme.foreground == foreground);
}

}  // namespace

int main()
{
    std::vector<uint8_t> buffer((kWidth / 4U) * kHeight, 0xFFU);
    Canvas canvas(kWidth, kHeight, buffer.data(), buffer.size());

    canvas.clear(GrayLevel::White);
    assert_theme(canvas, GrayLevel::White, GrayLevel::Black);

    canvas.clear(GrayLevel::Black);
    assert_theme(canvas, GrayLevel::Black, GrayLevel::White);

    canvas.clear(GrayLevel::Black);
    canvas.draw_pixel(kAreaX + 1, kAreaY + 1, GrayLevel::White);
    canvas.draw_pixel(kAreaX + kAreaWidth / 2,
                      kAreaY + 1,
                      GrayLevel::White);
    assert_theme(canvas, GrayLevel::Black, GrayLevel::White);

    canvas.clear(GrayLevel::White);
    canvas.draw_pixel(kAreaX + 1, kAreaY + 1, GrayLevel::Black);
    canvas.draw_pixel(kAreaX + kAreaWidth / 2,
                      kAreaY + 1,
                      GrayLevel::Black);
    assert_theme(canvas, GrayLevel::White, GrayLevel::Black);

    canvas.set_rotation(CanvasRotation::Deg180);
    canvas.clear(GrayLevel::Black);
    assert_theme(canvas, GrayLevel::Black, GrayLevel::White);
    return 0;
}
