#pragma once

#include <array>

struct TouchTestTarget {
    int x;
    int y;
    const char *name;
};

// Shared physical-screen coordinates for drawing and touch-result matching.
// 绘图和触摸结果匹配共用同一组屏幕物理坐标。
inline constexpr std::array<TouchTestTarget, 5> kTouchTestTargets = {{
    {85, 85, "TOP_LEFT"},
    {715, 85, "TOP_RIGHT"},
    {400, 240, "CENTER"},
    {85, 395, "BOTTOM_LEFT"},
    {715, 395, "BOTTOM_RIGHT"},
}};
