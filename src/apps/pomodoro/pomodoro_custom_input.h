#pragma once

#include <cstdint>

// Inserts one digit into the selected fixed-width time field.
// 向当前选中的定长时间字段写入一位数字。
inline void pomodoro_custom_enter_digit(uint8_t digit,
                                        uint8_t &value,
                                        bool &replace_field,
                                        bool &backspace_pending)
{
    if (backspace_pending) {
        value = static_cast<uint8_t>((value / 10U) * 10U + digit);
    } else if (replace_field) {
        value = digit;
    } else {
        value = static_cast<uint8_t>((value % 10U) * 10U + digit);
    }
    replace_field = false;
    backspace_pending = false;
}

// Clears the currently selected custom-duration field.
// 清空自定义时长中当前选中的字段。
inline void pomodoro_custom_clear_field(uint8_t &value,
                                        bool &replace_field,
                                        bool &backspace_pending)
{
    value = 0;
    replace_field = true;
    backspace_pending = false;
}

// Removes the last entered digit while preserving the fixed two-digit layout.
// 删除最后输入的一位，并保持固定的两位显示格式。
inline void pomodoro_custom_backspace(uint8_t &value,
                                      bool &replace_field,
                                      bool &backspace_pending)
{
    if (backspace_pending || value < 10U) {
        value = 0;
        replace_field = true;
        backspace_pending = false;
        return;
    }

    value = static_cast<uint8_t>((value / 10U) * 10U);
    replace_field = false;
    backspace_pending = true;
}
