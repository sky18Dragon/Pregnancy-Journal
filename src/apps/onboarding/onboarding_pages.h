#pragma once

#include <cstdint>

#include "onboarding_state.h"

class Canvas;

// Draws one portrait first-boot tutorial page from its approved bitmap.
// 使用已确认的位图绘制一页竖屏首次开机教程。
void onboarding_page_render(Canvas &canvas, uint8_t page_index);

// Maps one logical portrait coordinate to the tutorial footer action.
// 将竖屏逻辑触摸坐标映射为教程底部导航操作。
OnboardingAction onboarding_page_action_at(uint8_t page_index, int x, int y);
