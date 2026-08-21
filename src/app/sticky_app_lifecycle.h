#pragma once

#include <atomic>
#include <cstdint>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

struct StickyAppLifecycle {
    std::atomic<bool> pause_requested{false};
    std::atomic<bool> paused{false};
};

// Waits at a safe app-loop boundary while another page owns the display.
// 在APP循环的安全边界等待，直到屏幕使用权重新交回当前APP。
bool sticky_app_lifecycle_checkpoint(StickyAppLifecycle &lifecycle);

// Requests a cooperative pause and waits until the app reaches a safe boundary.
// 请求协作式暂停，并等待APP运行到安全边界。
esp_err_t sticky_app_lifecycle_pause(StickyAppLifecycle &lifecycle,
                                     TaskHandle_t task,
                                     uint32_t timeout_ms = 5000U);

// Releases a paused app; the app redraws its current page after waking.
// 恢复暂停的APP，APP醒来后会重新绘制当前页面。
void sticky_app_lifecycle_resume(StickyAppLifecycle &lifecycle);
