#pragma once

#include "book_of_answers_state.h"

class Canvas;

enum class BookOfAnswersShakeFrame {
    Left,
    Right,
};

void book_of_answers_page_render_home(Canvas &canvas,
                                      BookOfAnswersMode mode);
void book_of_answers_page_render_shaking(Canvas &canvas,
                                         BookOfAnswersShakeFrame frame);
void book_of_answers_page_render_thinking(Canvas &canvas);
void book_of_answers_page_render_revealing(Canvas &canvas);
void book_of_answers_page_render_message_result(Canvas &canvas,
                                                const char *answer);
void book_of_answers_page_render_crystal_result(Canvas &canvas,
                                                const char *answer);

BookOfAnswersAction book_of_answers_page_action_at(BookOfAnswersPage page,
                                                   int x,
                                                   int y);
