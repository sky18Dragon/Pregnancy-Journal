#pragma once

class Canvas;

#if defined(STICKY_STORAGE_STATUS_TEST) && STICKY_STORAGE_STATUS_TEST
[[noreturn]] void sticky_storage_status_test_run(Canvas &canvas);
#else
inline void sticky_storage_status_test_run(Canvas &) {}
#endif
