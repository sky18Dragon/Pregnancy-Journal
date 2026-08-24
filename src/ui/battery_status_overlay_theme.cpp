#include "battery_status_overlay_theme.h"

#include <array>
#include <utility>

BatteryStatusOverlayTheme battery_status_overlay_theme(
    const Canvas &canvas,
    int area_x,
    int area_y,
    int area_width,
    int area_height)
{
    const int left = area_x + 1;
    const int center = area_x + area_width / 2;
    const int right = area_x + area_width - 2;
    const int top = area_y + 1;
    const int bottom = area_y + area_height - 2;
    const std::array<std::pair<int, int>, 6> samples = {{
        {left, top},
        {center, top},
        {right, top},
        {left, bottom},
        {center, bottom},
        {right, bottom},
    }};

    // Majority sampling keeps one nearby page decoration from changing theme.
    // 多点多数判断可避免附近单个页面装饰误触发配色切换。
    size_t dark_samples = 0U;
    for (const auto &sample : samples) {
        if (canvas.pixel_at(sample.first, sample.second) <=
            GrayLevel::DarkGray) {
            ++dark_samples;
        }
    }

    const bool dark_background = dark_samples * 2U >= samples.size();
    return dark_background
               ? BatteryStatusOverlayTheme{
                     GrayLevel::Black, GrayLevel::White}
               : BatteryStatusOverlayTheme{
                     GrayLevel::White, GrayLevel::Black};
}
