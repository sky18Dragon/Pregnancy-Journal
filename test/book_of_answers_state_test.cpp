#include <cassert>
#include <cstring>

#include "book_of_answers_answers.h"
#include "book_of_answers_state.h"

int main()
{
    BookOfAnswersState state = {};
    assert(state.page == BookOfAnswersPage::Home);
    assert(state.mode == BookOfAnswersMode::Message);

    assert(book_of_answers_state_handle_action(
        state, BookOfAnswersAction::SelectCrystal));
    assert(state.mode == BookOfAnswersMode::Crystal);
    assert(!book_of_answers_state_handle_action(
        state, BookOfAnswersAction::SelectCrystal));

    assert(book_of_answers_state_handle_action(
        state, BookOfAnswersAction::Start));
    assert(state.page == BookOfAnswersPage::Shaking);
    assert(!book_of_answers_state_handle_action(
        state, BookOfAnswersAction::End));

    assert(book_of_answers_state_advance(state));
    assert(state.page == BookOfAnswersPage::Thinking);
    assert(book_of_answers_state_advance(state));
    assert(state.page == BookOfAnswersPage::Revealing);
    assert(book_of_answers_state_advance(state));
    assert(state.page == BookOfAnswersPage::CrystalResult);
    assert(!book_of_answers_state_advance(state));

    assert(book_of_answers_state_handle_action(
        state, BookOfAnswersAction::AskAgain));
    assert(state.page == BookOfAnswersPage::Shaking);
    assert(state.mode == BookOfAnswersMode::Crystal);
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

    assert(book_message_answer_count() == 18U);
    assert(book_crystal_answer_count() == 3U);
    assert(std::strcmp(book_crystal_answer(0U), "YES") == 0);
    assert(std::strcmp(book_crystal_answer(1U), "NO") == 0);
    assert(std::strcmp(book_crystal_answer(2U), "UNCLEAR") == 0);
    return 0;
}
