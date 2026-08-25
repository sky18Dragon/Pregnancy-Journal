#include "onboarding_app.h"

#include <cstdint>

#include "app_log.h"
#include "canvas.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "onboarding_pages.h"
#include "onboarding_state.h"
#include "sticky_display.h"
#include "sticky_touch.h"

namespace {

constexpr char kTag[] = "onboarding_app";
constexpr char kNamespace[] = "sticky_ui";
constexpr char kCompletionKey[] = "tutorial_v2";
constexpr uint8_t kCompletedValue = 1U;
constexpr TickType_t kInputPollInterval = pdMS_TO_TICKS(20);

esp_err_t read_completion(bool &completed)
{
    completed = false;
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (result != ESP_OK) {
        return result;
    }

    uint8_t value = 0U;
    result = nvs_get_u8(handle, kCompletionKey, &value);
    nvs_close(handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (result == ESP_OK) {
        completed = value == kCompletedValue;
    }
    return result;
}

esp_err_t write_completion()
{
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        return result;
    }
    result = nvs_set_u8(handle, kCompletionKey, kCompletedValue);
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
    return result;
}

esp_err_t render_page(Canvas &canvas, uint8_t page_index, bool first_frame)
{
    onboarding_page_render(canvas, page_index);
    if (first_frame || page_index == 3U || page_index == 6U) {
        return sticky_display_refresh_monochrome();
    }
    return sticky_display_refresh_partial();
}

}  // namespace

esp_err_t onboarding_app_run_if_needed(Canvas &canvas)
{
    app_log_register_tag(kTag);
    bool completed = false;
    const esp_err_t read_result = read_completion(completed);
    if (read_result != ESP_OK) {
        STICKY_LOGW(kTag,
                    "tutorial=completion_read result=%s fallback=show",
                    esp_err_to_name(read_result));
    } else if (completed) {
        STICKY_LOGI(kTag, "tutorial=boot action=skip reason=completed");
        return ESP_OK;
    }

    sticky_display_set_battery_overlay_enabled(false);
    sticky_touch_clear_press();
    sticky_touch_clear_interaction();
    OnboardingState state = {};
    esp_err_t result = render_page(canvas, state.page_index, true);
    if (result != ESP_OK) {
        sticky_display_set_battery_overlay_enabled(true);
        return result;
    }
    STICKY_LOGI(kTag,
                "tutorial=opened page=1 pages=%u input=touch result=ok",
                static_cast<unsigned>(kOnboardingPageCount));

    while (!state.completed) {
        StickyTouchPress press = {};
        if (!sticky_touch_take_press(press)) {
            vTaskDelay(kInputPollInterval);
            continue;
        }

        int logical_x = 0;
        int logical_y = 0;
        canvas.physical_to_logical(
            press.x, press.y, logical_x, logical_y);
        const OnboardingAction action = onboarding_page_action_at(
            state.page_index, logical_x, logical_y);
        const uint8_t previous_page = state.page_index;
        if (!onboarding_state_apply(state, action)) {
            continue;
        }

        if (state.completed) {
            const esp_err_t save_result = write_completion();
            STICKY_LOGI(kTag,
                        "tutorial=completed action=%s page=%u save=%s",
                        action == OnboardingAction::Skip ? "skip" : "start",
                        static_cast<unsigned>(previous_page + 1U),
                        esp_err_to_name(save_result));
            result = save_result;
            break;
        }

        STICKY_LOGI(kTag,
                    "tutorial=navigate page_from=%u page_to=%u action=next",
                    static_cast<unsigned>(previous_page + 1U),
                    static_cast<unsigned>(state.page_index + 1U));
        result = render_page(canvas, state.page_index, false);
        if (result != ESP_OK) {
            break;
        }
    }

    sticky_touch_clear_press();
    sticky_touch_clear_interaction();
    sticky_display_set_battery_overlay_enabled(true);
    return result;
}

esp_err_t onboarding_app_reset_completion()
{
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        return result;
    }
    result = nvs_erase_key(handle, kCompletionKey);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        result = ESP_OK;
    }
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
    return result;
}
