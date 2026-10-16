#pragma once

#include <cstdint>

// How a page reacts to the hardware keys. Each page declares one through a pure
// `*_page_key_policy()` function; the app publishes the policy of the page it
// is showing and the coordinator's key router reads it.
// 页面对硬件按键的反应；各页面用纯函数声明，应用发布当前页的策略，由协调器的按键路由读取。
enum class KeyPolicy : uint8_t {
    Locked,   // side keys ignored; top key may still leave (page state is kept)
    Free,     // side keys switch app; top key goes to Baby Week home
    Home,     // Free, and this is the home page: top key opens the launcher
    Pager,    // side keys flip the page's own list; otherwise like Free
    Capture,  // top key press is the page's primary action; everything else ignored
    Wizard,   // setup flow with no home to return to: every key ignored
};
