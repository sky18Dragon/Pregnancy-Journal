#include <cassert>
#include <cstring>

#include "book_of_answers_answers.h"
#include "book_of_answers_state.h"

int main()
{
    BookOfAnswersState state = {};
    assert(state.page == BookOfAnswersPage::Home);
    assert(state.mode == BookOfAnswersMode::Message);
    assert(!book_of_answers_shake_qualified(2999U));
    assert(book_of_answers_shake_qualified(3000U));

    BookOfAnswersShakeInputGate input_gate = {};
    assert(!input_gate.waiting_for_quiet);
    book_of_answers_shake_input_require_fresh(input_gate);
    assert(input_gate.waiting_for_quiet);
    assert(!book_of_answers_shake_input_update(
        input_gate, true, 1000U));
    assert(!book_of_answers_shake_input_update(
        input_gate, false, 1200U));
    assert(!book_of_answers_shake_input_update(
        input_gate, false, 1699U));
    assert(book_of_answers_shake_input_update(
        input_gate, false, 1700U));
    assert(!input_gate.waiting_for_quiet);
    book_of_answers_shake_input_require_fresh(input_gate);
    book_of_answers_shake_input_allow_current(input_gate);
    assert(!input_gate.waiting_for_quiet);

    assert(book_of_answers_state_handle_action(
        state, BookOfAnswersAction::SelectCrystal));
    assert(state.mode == BookOfAnswersMode::Crystal);
    assert(!book_of_answers_state_handle_action(
        state, BookOfAnswersAction::SelectCrystal));

    assert(book_of_answers_state_handle_action(
        state, BookOfAnswersAction::ShakeStarted));
    assert(state.page == BookOfAnswersPage::Shaking);

    // Only stopping during the three-second shake stage routes to guidance.
    // 只有在三秒摇晃阶段提前停下，才会进入继续摇晃引导页。
    BookOfAnswersState stopped = state;
    assert(book_of_answers_state_handle_action(
        stopped, BookOfAnswersAction::ShakeStopped));
    assert(stopped.page == BookOfAnswersPage::ShakeLonger);
    stopped.page = BookOfAnswersPage::Thinking;
    assert(!book_of_answers_state_handle_action(
        stopped, BookOfAnswersAction::ShakeStopped));
    assert(!book_of_answers_state_handle_action(
        state, BookOfAnswersAction::End));

    assert(book_of_answers_state_handle_action(
        state, BookOfAnswersAction::ShakeStopped));
    assert(state.page == BookOfAnswersPage::ShakeLonger);
    BookOfAnswersState automatic_return = state;
    assert(book_of_answers_state_advance(automatic_return));
    assert(automatic_return.page == BookOfAnswersPage::Home);
    assert(book_of_answers_state_handle_action(
        state, BookOfAnswersAction::ShakeStarted));
    assert(state.page == BookOfAnswersPage::Shaking);

    assert(book_of_answers_state_advance(state));
    assert(state.page == BookOfAnswersPage::Thinking);
    assert(book_of_answers_state_advance(state));
    assert(state.page == BookOfAnswersPage::Revealing);
    assert(book_of_answers_state_advance(state));
    assert(state.page == BookOfAnswersPage::CrystalResult);
    assert(!book_of_answers_state_advance(state));

    assert(book_of_answers_state_handle_action(
        state, BookOfAnswersAction::AskAgain));
    assert(state.page == BookOfAnswersPage::Home);
    assert(state.mode == BookOfAnswersMode::Crystal);
    assert(book_of_answers_state_handle_action(
        state, BookOfAnswersAction::ShakeStarted));
    assert(book_of_answers_state_advance(state));
    assert(book_of_answers_state_advance(state));
    assert(book_of_answers_state_advance(state));
    assert(book_of_answers_state_handle_action(
        state, BookOfAnswersAction::End));
    assert(state.page == BookOfAnswersPage::Home);
    assert(state.mode == BookOfAnswersMode::Crystal);

    assert(book_of_answers_choose_index(10U, 0U, 0U) ==
           kBookOfAnswersNoIndex);
    assert(book_of_answers_choose_index(10U, 0U, 1U) == 0U);
    const size_t first = book_of_answers_choose_index(
        4U, kBookOfAnswersNoIndex, 3U);
    const size_t second = book_of_answers_choose_index(4U, first, 3U);
    assert(first < 3U);
    assert(second < 3U);
    assert(second != first);

    assert(book_message_answer_count() == 350U);
    assert(std::strcmp(book_message_answer(0U).text,
                       "YOU WILL NOT BE DISAPPOINTED") == 0);
    assert(std::strcmp(book_message_answer(228U).text,
                       "IT COULD MEAN THAT YOU MAY HAVE TO DO SOMETHING "
                       "THAT YOU'VE NEVER DONE") == 0);
    assert(std::strcmp(book_message_answer(349U).text,
                       "DON'T GET CAUGHT UP IN THE DETAILS") == 0);
    assert(book_crystal_answer_count() == 3U);
    assert(std::strcmp(book_crystal_answer(0U), "YES") == 0);
    assert(std::strcmp(book_crystal_answer(1U), "NO") == 0);
    assert(std::strcmp(book_crystal_answer(2U), "UNCLEAR") == 0);
    return 0;
}
