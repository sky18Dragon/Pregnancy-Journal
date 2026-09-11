#pragma once

#include "sticky_app_id.h"

class Canvas;

// Draws the touch-selectable application cards in the current rotation.
// 在当前屏幕方向绘制可触摸选择的应用卡片。
void app_page_render_launcher(Canvas &canvas, StickyAppId current_app);

// Maps the global language switch shown in the launcher's top-right corner.
// 映射启动器右上角的全局语言切换按钮。
bool app_page_launcher_language_at(int width, int height, int x, int y);

// Maps one logical touch coordinate to the card drawn at the same position.
// 将逻辑触摸坐标映射到同一位置绘制的应用卡片。
bool app_page_launcher_app_at(int width,
                              int height,
                              int x,
                              int y,
                              StickyAppId &selected_app);

// Maps the Debug-only guide icon while keeping a generous invisible hit area.
// 映射仅Debug版显示的教程图标，并保留较大的透明触发区域。
bool app_page_launcher_tutorial_at(int width, int height, int x, int y);
