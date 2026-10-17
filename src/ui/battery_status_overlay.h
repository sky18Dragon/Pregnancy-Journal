#pragma once

class Canvas;

// Clears the overlay footprint using its detected page background color.
// 使用检测到的页面底色清除普通布局或睡眠布局的电量显示区域。
void battery_status_overlay_clear(Canvas &canvas, bool sleep_layout);

// Draws a high-contrast cached battery percentage in the logical top-right.
// 在当前逻辑方向的右上角以高对比配色绘制缓存的电池百分比。
void battery_status_overlay_draw(Canvas &canvas, bool sleep_layout);

// Draws the compact battery used by the pregnancy home: the icon only, the fill shows the charge.
void battery_status_overlay_draw_compact(Canvas &canvas, int x, int y);

// Footprint of the compact indicator drawn at (x, y): a 44x22 battery plus its 4 px nub, no label.
// A page that places something next to it needs this.
constexpr int kBatteryStatusCompactWidth = 48;
constexpr int kBatteryStatusCompactHeight = 22;
