#pragma once

#include "canvas.h"

struct BatteryStatusOverlayTheme {
    GrayLevel background;
    GrayLevel foreground;
};

// Chooses a high-contrast palette from the reserved overlay background.
// 根据电量显示预留区域的底色选择高对比度配色。
BatteryStatusOverlayTheme battery_status_overlay_theme(
    const Canvas &canvas,
    int area_x,
    int area_y,
    int area_width,
    int area_height);
