#pragma once

#include <cstddef>

struct BookMessageAnswer {
    const char *text;
};

// Exposes the read-only message-answer table embedded in firmware.
// 提供固件内置的一句话答案只读表。
size_t book_message_answer_count();
const BookMessageAnswer &book_message_answer(size_t index);

// Exposes the three read-only crystal answers: YES, NO, and UNCLEAR.
// 提供水晶球的三个只读答案：YES、NO和UNCLEAR。
size_t book_crystal_answer_count();
const char *book_crystal_answer(size_t index);
