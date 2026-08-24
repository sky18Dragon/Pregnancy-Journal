#include <cassert>

#include "status_board_state.h"

int main()
{
    StatusBoardState state = {};
    assert(state.page == StatusBoardPage::Menu);

    assert(status_board_state_handle_action(
        state, StatusBoardAction::SelectBusy, false));
    assert(state.page == StatusBoardPage::Display);
    assert(state.selected_status == StatusBoardStatus::Busy);

    assert(status_board_state_handle_action(
        state, StatusBoardAction::Back, false));
    assert(state.page == StatusBoardPage::Menu);

    assert(status_board_state_handle_action(
        state, StatusBoardAction::SelectCustom, false));
    assert(state.page == StatusBoardPage::CustomInput);
    assert(state.selected_status == StatusBoardStatus::Busy);

    assert(!status_board_state_handle_action(
        state, StatusBoardAction::Apply, false));
    assert(state.page == StatusBoardPage::CustomInput);

    assert(status_board_state_handle_action(
        state, StatusBoardAction::Apply, true));
    assert(state.page == StatusBoardPage::Display);
    assert(state.selected_status == StatusBoardStatus::Custom);

    assert(status_board_state_handle_action(
        state, StatusBoardAction::Back, true));
    assert(state.page == StatusBoardPage::Menu);
    return 0;
}
