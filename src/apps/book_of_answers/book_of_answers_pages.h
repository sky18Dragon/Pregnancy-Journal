#pragma once

#include "book_of_answers_state.h"

class Canvas;

enum class BookOfAnswersShakeFrame {
    Left,
    Right,
};

enum class BookOfAnswersAnimationFrame {
    Primary,
    Secondary,
};

void book_of_answers_page_render_home(Canvas &canvas,
                                      BookOfAnswersMode mode,
                                      BookOfAnswersAnimationFrame frame);
void book_of_answers_page_render_shaking(Canvas &canvas,
                                         BookOfAnswersShakeFrame frame);
void book_of_answers_page_render_thinking(
    Canvas &canvas,
    BookOfAnswersAnimationFrame frame);
void book_of_answers_page_render_revealing(
    Canvas &canvas,
    BookOfAnswersAnimationFrame frame);
void book_of_answers_page_render_message_result(Canvas &canvas,
                                                const char *answer,
                                                BookOfAnswersAnimationFrame frame);
void book_of_answers_page_render_crystal_result(Canvas &canvas,
                                                const char *answer,
                                                BookOfAnswersAnimationFrame frame);

BookOfAnswersAction book_of_answers_page_action_at(BookOfAnswersPage page,
                                                   int x,
                                                   int y);
