#include "sticky_app_lifecycle.h"

bool sticky_app_lifecycle_checkpoint(StickyAppLifecycle &lifecycle)
{
    if (!lifecycle.pause_requested.load(std::memory_order_acquire)) {
        return false;
    }

    uint32_t request = 0U;
    while ((request = lifecycle.pause_requested.load(std::memory_order_acquire)) != 0U) {
        lifecycle.acknowledged.store(request, std::memory_order_release);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return true;
}

esp_err_t sticky_app_lifecycle_pause(StickyAppLifecycle &lifecycle,
                                     TaskHandle_t task,
                                     uint32_t timeout_ms)
{
    if (task == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    if (++lifecycle.next_request == 0U) ++lifecycle.next_request;
    const uint32_t request = lifecycle.next_request;
    lifecycle.pause_requested.store(request, std::memory_order_release);
    const TickType_t started_at = xTaskGetTickCount();
    const TickType_t timeout = pdMS_TO_TICKS(timeout_ms);
    while (lifecycle.acknowledged.load(std::memory_order_acquire) != request) {
        if (xTaskGetTickCount() - started_at >= timeout) {
            lifecycle.pause_requested.store(0U,
                                             std::memory_order_release);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return ESP_OK;
}

void sticky_app_lifecycle_resume(StickyAppLifecycle &lifecycle)
{
    lifecycle.pause_requested.store(0U, std::memory_order_release);
}
