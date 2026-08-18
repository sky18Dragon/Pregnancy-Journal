#include "status_board_app.h"

#include "app_log.h"
#include "canvas.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "status_board_pages.h"
#include "sticky_display.h"
#include "sticky_touch.h"

namespace {

constexpr char kTag[] = "status_board_app";
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(40);
constexpr uint32_t kTaskStackSize = 4096;
constexpr UBaseType_t kTaskPriority = 3;

Canvas *s_canvas = nullptr;
TaskHandle_t s_app_task = nullptr;
StatusBoardStatus s_selected_status = StatusBoardStatus::InMeeting;

void render_page(bool partial_refresh)
{
    status_board_page_render(*s_canvas, s_selected_status);
    const esp_err_t result = partial_refresh
                                 ? sticky_display_refresh_partial()
                                 : sticky_display_refresh_monochrome();
    if (result != ESP_OK) {
        STICKY_LOGE(kTag,
                    "status_board=display refresh=%s result=%s",
                    partial_refresh ? "partial" : "full",
                    esp_err_to_name(result));
    }
    sticky_touch_clear_press();
}

void handle_action(StatusBoardAction action)
{
    StatusBoardStatus next_status = s_selected_status;
    if (!status_board_action_status(action, next_status)) {
        return;
    }

    if (next_status == s_selected_status) {
        STICKY_LOGD(kTag,
                    "status_board=selection status=%s result=unchanged",
                    status_board_status_name(s_selected_status));
        return;
    }

    const StatusBoardStatus previous_status = s_selected_status;
    s_selected_status = next_status;
    STICKY_LOGI(kTag,
                "status_board=selection from=%s to=%s result=ok",
                status_board_status_name(previous_status),
                status_board_status_name(s_selected_status));
    render_page(true);
}

void app_task(void *)
{
    render_page(false);
    STICKY_LOGI(kTag,
                "status_board=ready orientation=landscape status=%s choices=6 result=ok",
                status_board_status_name(s_selected_status));

    while (true) {
        StickyTouchPress press = {};
        if (sticky_touch_take_press(press)) {
            int logical_x = 0;
            int logical_y = 0;
            s_canvas->physical_to_logical(
                press.x, press.y, logical_x, logical_y);
            const StatusBoardAction action =
                status_board_page_action_at(logical_x, logical_y);
            if (action != StatusBoardAction::None) {
                STICKY_LOGI(kTag,
                            "status_board=touch action=%s physical_x=%u physical_y=%u logical_x=%d logical_y=%d",
                            status_board_action_name(action),
                            static_cast<unsigned>(press.x),
                            static_cast<unsigned>(press.y),
                            logical_x,
                            logical_y);
                handle_action(action);
            }
        }

        vTaskDelay(kPollInterval);
    }
}

}  // namespace

esp_err_t status_board_app_start(Canvas &canvas)
{
    app_log_register_tag(kTag);
    if (s_app_task != nullptr) {
        return ESP_OK;
    }

    s_canvas = &canvas;
    if (xTaskCreate(app_task,
                    "status_board_app",
                    kTaskStackSize,
                    nullptr,
                    kTaskPriority,
                    &s_app_task) != pdPASS) {
        s_canvas = nullptr;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
