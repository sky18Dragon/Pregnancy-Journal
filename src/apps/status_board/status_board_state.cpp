#include "status_board_state.h"

bool status_board_state_handle_action(StatusBoardState &state,
                                      StatusBoardAction action,
                                      bool custom_text_valid)
{
    if (state.page == StatusBoardPage::Menu) {
        if (action == StatusBoardAction::SelectCustom) {
            state.page = StatusBoardPage::CustomInput;
            return true;
        }

        StatusBoardStatus status = state.selected_status;
        if (status_board_action_status(action, status)) {
            state.selected_status = status;
            state.page = StatusBoardPage::Display;
            return true;
        }
        return false;
    }

    if (state.page == StatusBoardPage::Display) {
        if (action == StatusBoardAction::Back) {
            state.page = StatusBoardPage::Menu;
            return true;
        }
        return false;
    }

    if (action == StatusBoardAction::Back) {
        state.page = StatusBoardPage::Menu;
        return true;
    }
    if (action == StatusBoardAction::Apply && custom_text_valid) {
        state.selected_status = StatusBoardStatus::Custom;
        state.page = StatusBoardPage::Display;
        return true;
    }
    return false;
}
