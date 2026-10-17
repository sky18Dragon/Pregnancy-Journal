#pragma once

class Canvas;

// Clears the overlay footprint using its detected page background color.
// 使用检测到的页面底色清除普通布局或睡眠布局的电量显示区域。
void battery_status_overlay_clear(Canvas &canvas, bool sleep_layout);

// Draws a high-contrast cached battery percentage in the logical top-right.
// 在当前逻辑方向的右上角以高对比配色绘制缓存的电池百分比。
void battery_status_overlay_draw(Canvas &canvas, bool sleep_layout);

// Draws the compact battery treatment used by the pregnancy-home prototype.
void battery_status_overlay_draw_compact(Canvas &canvas, int x, int y);

// Width of the compact indicator drawn at (x, y): the label, then the 34 px battery and its nub.
// A page that places it right-aligned next to another control needs this.
constexpr int kBatteryStatusCompactWidth = 94;
