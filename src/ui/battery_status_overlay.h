#pragma once

class Canvas;

// Clears the overlay footprint using its detected page background color. The sleep layout's
// footprint is the page's header action button slot.
// 使用检测到的页面底色清除普通布局或睡眠布局的电量显示区域；睡眠布局的区域即页头按钮位置。
void battery_status_overlay_clear(Canvas &canvas, bool sleep_layout);

// Normal layout: cached battery percentage in the logical top-right. Sleep layout: a battery icon
// and a moon, no number, in the header action button slot.
// 普通布局：右上角电量百分比；休眠布局：页头按钮位置的电池图标加月亮，不带数字。
void battery_status_overlay_draw(Canvas &canvas, bool sleep_layout);

// Draws the compact battery used by the pregnancy home: the icon only, the fill shows the charge.
void battery_status_overlay_draw_compact(Canvas &canvas, int x, int y);

// Footprint of the compact indicator drawn at (x, y): a 44x22 battery plus its 4 px nub, no label.
// A page that places something next to it needs this.
constexpr int kBatteryStatusCompactWidth = 48;  // = kBatteryIconWidth (status_glyphs.h)
constexpr int kBatteryStatusCompactHeight = 22; // = kBatteryIconHeight
