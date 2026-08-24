#include <cassert>
#include <cstdint>

#include "pomodoro_custom_input.h"

int main()
{
    bool replace_field = false;
    bool backspace_pending = false;
    uint8_t value = 23U;

    pomodoro_custom_clear_field(
        value, replace_field, backspace_pending);
    assert(value == 0U);
    assert(replace_field);
    assert(!backspace_pending);

    value = 23U;
    replace_field = false;
    pomodoro_custom_backspace(
        value, replace_field, backspace_pending);
    assert(value == 20U);
    assert(!replace_field);
    assert(backspace_pending);

    pomodoro_custom_enter_digit(
        4U, value, replace_field, backspace_pending);
    assert(value == 24U);
    assert(!replace_field);
    assert(!backspace_pending);

    pomodoro_custom_backspace(
        value, replace_field, backspace_pending);
    assert(value == 20U);
    pomodoro_custom_backspace(
        value, replace_field, backspace_pending);
    assert(value == 0U);
    assert(replace_field);

    pomodoro_custom_enter_digit(
        5U, value, replace_field, backspace_pending);
    assert(value == 5U);
    return 0;
}
