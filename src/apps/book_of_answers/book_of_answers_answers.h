#pragma once

#include <cstddef>

struct BookMessageAnswer {
    const char *first_line;
    const char *second_line;
};

size_t book_message_answer_count();
const BookMessageAnswer &book_message_answer(size_t index);

size_t book_crystal_answer_count();
const char *book_crystal_answer(size_t index);
