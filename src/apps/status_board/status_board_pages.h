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

enum class StatusBoardAction : uint8_t {
    None,
    SelectFocusing,
    SelectInMeeting,
    SelectWelcome,
    SelectOutForLunch,
    SelectOffDuty,
    SelectCustom,
};

// Draws the complete 800x480 landscape status selector.
// 绘制完整的800x480横屏状态选择页面。
void status_board_page_render(Canvas &canvas, StatusBoardStatus selected_status);

// Maps one landscape touch coordinate to one of the six status actions.
// 将横屏触摸坐标映射到六个状态操作之一。
StatusBoardAction status_board_page_action_at(int x, int y);

// Converts a selection action into its corresponding status.
// 将选择操作转换为对应的状态。
bool status_board_action_status(StatusBoardAction action,
                                StatusBoardStatus &status);

const char *status_board_status_name(StatusBoardStatus status);
const char *status_board_action_name(StatusBoardAction action);
