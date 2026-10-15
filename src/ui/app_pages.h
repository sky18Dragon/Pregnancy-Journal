#pragma once

#include <cstddef>

#include "prototype_ui.h"
#include "sticky_app_id.h"

class Canvas;

// Draws the touch-selectable application cards in the current rotation.
// 在当前屏幕方向绘制可触摸选择的应用卡片。
void app_page_render_launcher(Canvas &canvas, StickyAppId current_app);

// One tappable launcher region: the close button or an application card.
struct LauncherTarget {
  prototype_ui::Rect rect;
  bool close;
  StickyAppId app;
};

// Lists the launcher's hit targets (close button first, then the cards in
// order). The renderer draws the same rectangles; app_page_launcher_close_at()
// and app_page_launcher_app_at() are derived from this list.
size_t app_page_launcher_targets(int width, int height, LauncherTarget *out,
                                 size_t capacity);

// Maps the prototype close button in the launcher's top-right corner.
bool app_page_launcher_close_at(int width, int height, int x, int y);

// Maps one logical touch coordinate to the card drawn at the same position.
// 将逻辑触摸坐标映射到同一位置绘制的应用卡片。
bool app_page_launcher_app_at(int width, int height, int x, int y,
                              StickyAppId &selected_app);
