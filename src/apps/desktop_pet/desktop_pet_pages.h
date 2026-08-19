#pragma once

#include "desktop_pet_state.h"

class Canvas;

// Draws the portrait home scene for the current pose and dialogue.
// 根据当前动作姿势和对白绘制竖屏桌宠主页。
void desktop_pet_page_render_home(Canvas &canvas,
                                  const DesktopPetState &state,
                                  DesktopPetPose pose,
                                  DesktopPetIdleFrame idle_frame,
                                  const char *message);

// Draws the accelerated-build control panel.
// 绘制加速测试固件的控制面板。
void desktop_pet_page_render_test(Canvas &canvas,
                                  const DesktopPetState &state,
                                  bool reset_confirmation);

// Renders one full-page frame of the Hatchling-to-Child transition.
// 渲染幼兔成长为儿童期的一帧全屏过场。
void desktop_pet_page_render_evolution(
    Canvas &canvas,
    const DesktopPetState &state,
    DesktopPetEvolutionFrame frame);

// Draws the final three-way care choice before the Youth evolution.
// 在进入青年期前绘制最终的三选一照料页。
void desktop_pet_page_render_personality_choice(
    Canvas &canvas,
    const DesktopPetState &state);

DesktopPetAction desktop_pet_page_action_at(bool test_open, int x, int y);
DesktopPetAction desktop_pet_page_personality_action_at(int x, int y);
