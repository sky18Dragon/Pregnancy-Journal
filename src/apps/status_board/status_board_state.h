#pragma once

#include "status_board_pages.h"

struct StatusBoardState {
    StatusBoardPage page = StatusBoardPage::Menu;
    StatusBoardStatus selected_status = StatusBoardStatus::Meeting;
};

// Applies navigation and status-selection actions to the app state.
// 将页面导航和状态选择操作应用到APP状态。
bool status_board_state_handle_action(StatusBoardState &state,
                                      StatusBoardAction action,
                                      bool custom_text_valid);
