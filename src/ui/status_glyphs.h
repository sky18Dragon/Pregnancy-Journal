#pragma once

#include "canvas.h"

// Status glyphs shared by the pregnancy home page and the sleep frame. Pure drawing: no hardware.
// 状态图形：孕周首页与休眠帧共用，只负责绘制，不访问硬件。

constexpr int kBatteryIconWidth = 48;  // 44 px body + 4 px nub
constexpr int kBatteryIconHeight = 22;
constexpr int kMoonIconSize = 22;      // visible extent of the crescent

// Battery whose fill alone shows the charge: 2 px frame, 2 px gap, fill proportional to `percent`.
// `percent` < 0 means unknown (frame only). A charging bolt is drawn over the fill. (x, y) is the
// top-left of the icon.
void battery_icon_draw(Canvas &canvas, int x, int y, int percent, bool charging,
                       GrayLevel foreground, GrayLevel background);

// Two-pixel-wide charging bolt, 8 x 16 px, top-left at (x, y).
void battery_bolt_draw(Canvas &canvas, int x, int y, GrayLevel color);

// Crescent moon centred on (cx, cy), kMoonIconSize px square. Paints only the crescent's pixels in
// `foreground`; `background` is unused and kept for symmetry with the other glyphs.
void moon_icon_draw(Canvas &canvas, int cx, int cy, GrayLevel foreground, GrayLevel background);

// Sleep status: centres the battery icon and the moon side by side in the slot. The slot is the
// page's header action button, which cannot be pressed while the device sleeps: a button there is
// cleared as a whole; on a page without one only the status's own footprint is wiped.
// 休眠状态：电池图标与月亮并排居中于槽位。槽位即页头按钮位置（休眠时按钮不可用）：有按钮则整块清空；
// 页面没有按钮时只清空状态自身占的区域。
void sleep_status_draw(Canvas &canvas, int slot_x, int slot_y, int slot_width, int slot_height,
                       int percent, bool charging, GrayLevel foreground, GrayLevel background);
