#pragma once

#include "sticky_app_id.h"

class Canvas;

// Draws the four touch-selectable application cards in the current rotation.
// 在当前屏幕方向绘制四张可触摸选择的应用卡片。
void app_page_render_launcher(Canvas &canvas, StickyAppId current_app);

// Maps one logical touch coordinate to the card drawn at the same position.
// 将逻辑触摸坐标映射到同一位置绘制的应用卡片。
bool app_page_launcher_app_at(int width,
                              int height,
                              int x,
                              int y,
                              StickyAppId &selected_app);
