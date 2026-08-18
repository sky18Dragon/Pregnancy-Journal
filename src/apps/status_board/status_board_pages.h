#pragma once

#include <cstdint>

class Canvas;

enum class StatusBoardStatus : uint8_t {
    Focusing,
    InMeeting,
    Welcome,
    OutForLunch,
    OffDuty,
    Custom,
};

enum class StatusBoardPage : uint8_t {
    Menu,
    Display,
    CustomInput,
};

enum class StatusBoardKeyboardMode : uint8_t {
    Letters,
    Numbers,
};

enum class StatusBoardAction : uint8_t {
    None,
    SelectFocusing,
    SelectInMeeting,
    SelectWelcome,
    SelectOutForLunch,
    SelectOffDuty,
    SelectCustom,
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

// Draws the first-level menu containing all six statuses.
// 绘制包含六种状态的一级选择菜单。
void status_board_page_render_menu(Canvas &canvas,
                                   StatusBoardStatus selected_status);

// Draws one selected status as a full-screen second-level page.
// 将选中的状态绘制为全屏二级页面。
void status_board_page_render_display(Canvas &canvas,
                                      StatusBoardStatus selected_status,
                                      const char *custom_text);

// Draws the on-device custom-status editor and its active keyboard.
// 绘制设备端自定义状态编辑器及当前键盘。
void status_board_page_render_custom_input(
    Canvas &canvas,
    const char *text,
    StatusBoardKeyboardMode keyboard_mode,
    bool input_error);

// Maps one landscape touch coordinate using the visible page and keyboard.
// 根据当前页面和键盘，将横屏触摸坐标映射为操作。
StatusBoardAction status_board_page_action_at(
    StatusBoardPage page,
    StatusBoardKeyboardMode keyboard_mode,
    int x,
    int y);

// Converts a selection action into its corresponding status.
// 将选择操作转换为对应的状态。
bool status_board_action_status(StatusBoardAction action,
                                StatusBoardStatus &status);

// Returns the character produced by a letter or digit key.
// 返回字母键或数字键对应的字符。
bool status_board_action_character(StatusBoardAction action, char &character);

// Returns whether consecutive actions can share one display refresh.
// 返回连续操作是否可以共用一次屏幕刷新。
bool status_board_action_can_batch(StatusBoardAction action);

const char *status_board_page_name(StatusBoardPage page);
const char *status_board_status_name(StatusBoardStatus status);
const char *status_board_action_name(StatusBoardAction action);
