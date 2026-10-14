#pragma once

class Canvas;

#if defined(STICKY_JOURNAL_POWER_TEST) && STICKY_JOURNAL_POWER_TEST
[[noreturn]] void sticky_journal_power_test_run(Canvas &canvas);
#endif
