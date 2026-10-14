#pragma once

class Canvas;

// Runs the interactive S0 input-page firmware until the device is reset.
// This profile does not persist or publish the entered test text.
[[noreturn]] void sticky_chinese_input_test_run(Canvas &canvas);
