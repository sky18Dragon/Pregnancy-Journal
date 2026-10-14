#pragma once

class Canvas;

#if defined(STICKY_OBJECT_POWER_TEST) && STICKY_OBJECT_POWER_TEST
[[noreturn]] void sticky_object_power_test_run(Canvas &canvas);
#endif
