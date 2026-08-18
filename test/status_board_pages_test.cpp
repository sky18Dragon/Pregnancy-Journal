#include <cassert>

#include "status_board_pages.h"

int main()
{
    assert(status_board_page_action_at(79, 365) ==
           StatusBoardAction::SelectFocusing);
    assert(status_board_page_action_at(207, 365) ==
           StatusBoardAction::SelectInMeeting);
    assert(status_board_page_action_at(335, 365) ==
           StatusBoardAction::SelectWelcome);
    assert(status_board_page_action_at(463, 365) ==
           StatusBoardAction::SelectOutForLunch);
    assert(status_board_page_action_at(591, 365) ==
           StatusBoardAction::SelectOffDuty);
    assert(status_board_page_action_at(719, 365) ==
           StatusBoardAction::SelectCustom);

    assert(status_board_page_action_at(10, 365) == StatusBoardAction::None);
    assert(status_board_page_action_at(400, 200) == StatusBoardAction::None);
    assert(status_board_page_action_at(800, 479) == StatusBoardAction::None);

    StatusBoardStatus status = StatusBoardStatus::Focusing;
    assert(status_board_action_status(
        StatusBoardAction::SelectOutForLunch, status));
    assert(status == StatusBoardStatus::OutForLunch);
    assert(!status_board_action_status(StatusBoardAction::None, status));
    return 0;
}
