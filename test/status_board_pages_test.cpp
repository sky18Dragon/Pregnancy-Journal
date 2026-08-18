#include <cassert>

#include "status_board_pages.h"

int main()
{
    assert(status_board_page_action_at(StatusBoardPage::Menu,
                                       StatusBoardKeyboardMode::Letters,
                                       79,
                                       250) ==
           StatusBoardAction::SelectFocusing);
    assert(status_board_page_action_at(StatusBoardPage::Menu,
                                       StatusBoardKeyboardMode::Letters,
                                       207,
                                       250) ==
           StatusBoardAction::SelectInMeeting);
    assert(status_board_page_action_at(StatusBoardPage::Menu,
                                       StatusBoardKeyboardMode::Letters,
                                       335,
                                       250) ==
           StatusBoardAction::SelectWelcome);
    assert(status_board_page_action_at(StatusBoardPage::Menu,
                                       StatusBoardKeyboardMode::Letters,
                                       463,
                                       250) ==
           StatusBoardAction::SelectOutForLunch);
    assert(status_board_page_action_at(StatusBoardPage::Menu,
                                       StatusBoardKeyboardMode::Letters,
                                       591,
                                       250) ==
           StatusBoardAction::SelectOffDuty);
    assert(status_board_page_action_at(StatusBoardPage::Menu,
                                       StatusBoardKeyboardMode::Letters,
                                       719,
                                       250) ==
           StatusBoardAction::SelectCustom);

    assert(status_board_page_action_at(StatusBoardPage::Display,
                                       StatusBoardKeyboardMode::Letters,
                                       60,
                                       45) == StatusBoardAction::Back);
    assert(status_board_page_action_at(StatusBoardPage::Display,
                                       StatusBoardKeyboardMode::Letters,
                                       400,
                                       200) == StatusBoardAction::None);

    assert(status_board_page_action_at(StatusBoardPage::CustomInput,
                                       StatusBoardKeyboardMode::Letters,
                                       50,
                                       205) == StatusBoardAction::KeyQ);
    assert(status_board_page_action_at(StatusBoardPage::CustomInput,
                                       StatusBoardKeyboardMode::Letters,
                                       740,
                                       205) == StatusBoardAction::KeyP);
    assert(status_board_page_action_at(StatusBoardPage::CustomInput,
                                       StatusBoardKeyboardMode::Letters,
                                       680,
                                       430) == StatusBoardAction::Apply);
    assert(status_board_page_action_at(StatusBoardPage::CustomInput,
                                       StatusBoardKeyboardMode::Numbers,
                                       50,
                                       280) == StatusBoardAction::Digit1);
    assert(status_board_page_action_at(StatusBoardPage::CustomInput,
                                       StatusBoardKeyboardMode::Numbers,
                                       740,
                                       280) == StatusBoardAction::Digit0);

    assert(status_board_page_action_at(StatusBoardPage::Menu,
                                       StatusBoardKeyboardMode::Letters,
                                       10,
                                       250) == StatusBoardAction::None);
    assert(status_board_page_action_at(StatusBoardPage::Menu,
                                       StatusBoardKeyboardMode::Letters,
                                       800,
                                       479) == StatusBoardAction::None);

    StatusBoardStatus status = StatusBoardStatus::Focusing;
    assert(status_board_action_status(
        StatusBoardAction::SelectOutForLunch, status));
    assert(status == StatusBoardStatus::OutForLunch);
    assert(!status_board_action_status(StatusBoardAction::None, status));

    char character = '\0';
    assert(status_board_action_character(StatusBoardAction::KeyA, character));
    assert(character == 'A');
    assert(status_board_action_character(StatusBoardAction::KeyZ, character));
    assert(character == 'Z');
    assert(status_board_action_character(StatusBoardAction::Digit0, character));
    assert(character == '0');
    assert(status_board_action_character(StatusBoardAction::Digit9, character));
    assert(character == '9');
    assert(!status_board_action_character(StatusBoardAction::Apply, character));

    assert(status_board_action_can_batch(StatusBoardAction::KeyA));
    assert(status_board_action_can_batch(StatusBoardAction::Digit5));
    assert(status_board_action_can_batch(StatusBoardAction::Space));
    assert(status_board_action_can_batch(StatusBoardAction::Delete));
    assert(status_board_action_can_batch(StatusBoardAction::Clear));
    assert(!status_board_action_can_batch(StatusBoardAction::Back));
    assert(!status_board_action_can_batch(StatusBoardAction::Apply));
    assert(!status_board_action_can_batch(
        StatusBoardAction::ToggleKeyboard));
    return 0;
}
