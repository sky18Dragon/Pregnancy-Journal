#include "book_of_answers_state.h"

bool book_of_answers_state_handle_action(BookOfAnswersState &state,
                                         BookOfAnswersAction action)
{
    if (state.page == BookOfAnswersPage::Home) {
        if (action == BookOfAnswersAction::SelectMessage &&
            state.mode != BookOfAnswersMode::Message) {
            state.mode = BookOfAnswersMode::Message;
            return true;
        }
        if (action == BookOfAnswersAction::SelectCrystal &&
            state.mode != BookOfAnswersMode::Crystal) {
            state.mode = BookOfAnswersMode::Crystal;
            return true;
        }
        if (action == BookOfAnswersAction::ShakeStarted) {
            state.page = BookOfAnswersPage::Shaking;
            return true;
        }
        return false;
    }

    const bool animation_page =
        state.page == BookOfAnswersPage::Shaking ||
        state.page == BookOfAnswersPage::Thinking ||
        state.page == BookOfAnswersPage::Revealing;
    if (animation_page && action == BookOfAnswersAction::ShakeStopped) {
        state.page = BookOfAnswersPage::ShakeLonger;
        return true;
    }

    if (state.page == BookOfAnswersPage::ShakeLonger &&
        action == BookOfAnswersAction::ShakeStarted) {
        state.page = BookOfAnswersPage::Shaking;
        return true;
    }

    const bool result_page =
        state.page == BookOfAnswersPage::MessageResult ||
        state.page == BookOfAnswersPage::CrystalResult;
    if (!result_page) {
        return false;
    }

    if (action == BookOfAnswersAction::AskAgain) {
        state.page = BookOfAnswersPage::Home;
        return true;
    }
    if (action == BookOfAnswersAction::End) {
        state.page = BookOfAnswersPage::Home;
        return true;
    }
    return false;
}

bool book_of_answers_state_advance(BookOfAnswersState &state)
{
    switch (state.page) {
    case BookOfAnswersPage::Shaking:
        state.page = BookOfAnswersPage::Thinking;
        return true;
    case BookOfAnswersPage::Thinking:
        state.page = BookOfAnswersPage::Revealing;
        return true;
    case BookOfAnswersPage::Revealing:
        state.page = state.mode == BookOfAnswersMode::Message
                         ? BookOfAnswersPage::MessageResult
                         : BookOfAnswersPage::CrystalResult;
        return true;
    case BookOfAnswersPage::ShakeLonger:
        state.page = BookOfAnswersPage::Home;
        return true;
    case BookOfAnswersPage::Home:
    case BookOfAnswersPage::MessageResult:
    case BookOfAnswersPage::CrystalResult:
        return false;
    }
    return false;
}

size_t book_of_answers_choose_index(uint32_t random_value,
                                    size_t previous_index,
                                    size_t option_count)
{
    if (option_count == 0U) {
        return kBookOfAnswersNoIndex;
    }
    if (option_count == 1U) {
        return 0U;
    }

    size_t selected = static_cast<size_t>(random_value) % option_count;
    if (selected == previous_index && previous_index < option_count) {
        selected = (selected + 1U) % option_count;
    }
    return selected;
}

const char *book_of_answers_page_name(BookOfAnswersPage page)
{
    switch (page) {
    case BookOfAnswersPage::Home:
        return "home";
    case BookOfAnswersPage::Shaking:
        return "shaking";
    case BookOfAnswersPage::Thinking:
        return "thinking";
    case BookOfAnswersPage::Revealing:
        return "revealing";
    case BookOfAnswersPage::ShakeLonger:
        return "shake_longer";
    case BookOfAnswersPage::MessageResult:
        return "message_result";
    case BookOfAnswersPage::CrystalResult:
        return "crystal_result";
    }
    return "unknown";
}

const char *book_of_answers_mode_name(BookOfAnswersMode mode)
{
    switch (mode) {
    case BookOfAnswersMode::Message:
        return "message";
    case BookOfAnswersMode::Crystal:
        return "crystal";
    }
    return "unknown";
}

const char *book_of_answers_action_name(BookOfAnswersAction action)
{
    switch (action) {
    case BookOfAnswersAction::SelectMessage:
        return "select_message";
    case BookOfAnswersAction::SelectCrystal:
        return "select_crystal";
    case BookOfAnswersAction::ShakeStarted:
        return "shake_started";
    case BookOfAnswersAction::ShakeStopped:
        return "shake_stopped";
    case BookOfAnswersAction::AskAgain:
        return "ask_again";
    case BookOfAnswersAction::End:
        return "end";
    case BookOfAnswersAction::None:
    default:
        return "none";
    }
}
