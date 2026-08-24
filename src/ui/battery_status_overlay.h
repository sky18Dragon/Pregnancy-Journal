#pragma once

class Canvas;

// Clears the overlay footprint for either the normal or sleep-status layout.
// 清除普通布局或睡眠状态布局所占用的电量显示区域。
void battery_status_overlay_clear(Canvas &canvas, bool sleep_layout);

// Draws the cached battery percentage in the logical top-right corner.
// 在当前逻辑方向的右上角绘制缓存的电池百分比。
void battery_status_overlay_draw(Canvas &canvas, bool sleep_layout);
