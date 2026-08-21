#include "sticky_app_lifecycle.h"

bool sticky_app_lifecycle_checkpoint(StickyAppLifecycle &lifecycle)
{
    if (!lifecycle.pause_requested.load(std::memory_order_acquire)) {
        return false;
    }

    lifecycle.paused.store(true, std::memory_order_release);
    while (lifecycle.pause_requested.load(std::memory_order_acquire)) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    lifecycle.paused.store(false, std::memory_order_release);
    return true;
}

esp_err_t sticky_app_lifecycle_pause(StickyAppLifecycle &lifecycle,
                                     TaskHandle_t task,
                                     uint32_t timeout_ms)
{
    if (task == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    lifecycle.pause_requested.store(true, std::memory_order_release);
    const TickType_t started_at = xTaskGetTickCount();
    const TickType_t timeout = pdMS_TO_TICKS(timeout_ms);
    while (!lifecycle.paused.load(std::memory_order_acquire)) {
        if (xTaskGetTickCount() - started_at >= timeout) {
            lifecycle.pause_requested.store(false,
                                             std::memory_order_release);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return ESP_OK;
}

void sticky_app_lifecycle_resume(StickyAppLifecycle &lifecycle)
{
    lifecycle.pause_requested.store(false, std::memory_order_release);
}
