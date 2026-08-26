#pragma once

#include "desktop_pet_outing.h"
#include "desktop_pet_state.h"

class Canvas;

enum class DesktopPetKeyboardMode : uint8_t {
    Letters,
    Numbers,
};

enum class DesktopPetNameAction : uint8_t {
    None,
    Back,
    KeyA,
    KeyB,
    KeyC,
    KeyD,
    KeyE,
    KeyF,
    KeyG,
    KeyH,
    KeyI,
    KeyJ,
    KeyK,
    KeyL,
    KeyM,
    KeyN,
    KeyO,
    KeyP,
    KeyQ,
    KeyR,
    KeyS,
    KeyT,
    KeyU,
    KeyV,
    KeyW,
    KeyX,
    KeyY,
    KeyZ,
    Digit0,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
    ToggleKeyboard,
    Space,
    Delete,
    Clear,
    Apply,
};

enum class DesktopPetCelebrationFrame : uint8_t {
    Proud,
    Jump,
};

// Draws the full-screen egg and one frame of its tap-to-hatch sequence.
// 绘制全屏宠物蛋，以及轻触孵化过程中的一帧。
void desktop_pet_page_render_egg(Canvas &canvas,
                                 const DesktopPetState &state,
                                 DesktopPetHatchFrame frame);

// Draws the portrait home scene for the current pose and dialogue.
// 根据当前动作姿势和对白绘制竖屏桌宠主页。
void desktop_pet_page_render_home(Canvas &canvas,
                                  const DesktopPetState &state,
                                  DesktopPetPose pose,
                                  DesktopPetIdleFrame idle_frame,
                                  const char *message);

// Draws one full nighttime sleep scene and its current energy progress.
// 绘制一帧完整夜间睡眠场景及当前精力恢复进度。
void desktop_pet_page_render_sleep(Canvas &canvas,
                                   const DesktopPetState &state,
                                   bool secondary_frame);

// Draws one complete scene from the rabbit's outing sequence.
// 绘制兔子外出流程中的一幅完整场景。
void desktop_pet_page_render_outing(
    Canvas &canvas,
    const DesktopPetState &state,
    const DesktopPetOutingSession &outing,
    bool secondary_frame);

// Draws the accelerated-build control panel.
// 绘制加速测试固件的控制面板。
void desktop_pet_page_render_test(Canvas &canvas,
                                  const DesktopPetState &state,
                                  bool reset_confirmation);

// Renders one full-page frame for the active life-stage transition.
// 渲染当前成长阶段切换的一帧全屏过场。
void desktop_pet_page_render_evolution(
    Canvas &canvas,
    const DesktopPetState &state,
    DesktopPetEvolutionFrame frame);

// Draws one full-screen care-streak celebration frame.
// 绘制一帧全屏连续照料庆祝画面。
void desktop_pet_page_render_care_celebration(
    Canvas &canvas,
    const DesktopPetState &state,
    uint16_t milestone_days,
    DesktopPetCelebrationFrame frame);

// Draws the final three-way care choice before the Youth evolution.
// 在进入青年期前绘制最终的三选一照料页。
void desktop_pet_page_render_personality_choice(
    Canvas &canvas,
    const DesktopPetState &state);

// Draws the portrait pet-name editor with a large touch keyboard.
// 绘制带大触摸键盘的竖屏宠物命名页面。
void desktop_pet_page_render_name_editor(
    Canvas &canvas,
    const char *text,
    DesktopPetKeyboardMode keyboard_mode,
    bool input_error,
    bool can_cancel);

DesktopPetAction desktop_pet_page_action_at(bool test_open, int x, int y);
DesktopPetAction desktop_pet_page_egg_action_at(int x, int y);
DesktopPetAction desktop_pet_page_personality_action_at(int x, int y);
DesktopPetAction desktop_pet_page_sleep_action_at(int x, int y);
DesktopPetAction desktop_pet_page_outing_action_at(
    DesktopPetOutingPhase phase,
    int x,
    int y);

// Maps one portrait keyboard touch coordinate into a name-editor action.
// 将竖屏键盘触摸坐标映射为命名编辑操作。
DesktopPetNameAction desktop_pet_page_name_action_at(
    DesktopPetKeyboardMode keyboard_mode,
    bool can_cancel,
    int x,
    int y);

// Converts editor actions into characters and refresh-batching decisions.
// 将编辑操作转换为字符，并判断是否可以合并屏幕刷新。
bool desktop_pet_name_action_character(DesktopPetNameAction action,
                                       char &character);
bool desktop_pet_name_action_can_batch(DesktopPetNameAction action);
const char *desktop_pet_name_action_name(DesktopPetNameAction action);
