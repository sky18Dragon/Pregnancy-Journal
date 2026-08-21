#pragma once

class Canvas;

// Draws the cached battery percentage in the logical top-right corner.
// 在当前逻辑方向的右上角绘制缓存的电池百分比。
void battery_status_overlay_draw(Canvas &canvas);
