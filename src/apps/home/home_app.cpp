#include "home_app.h"

#include <atomic>

#include "app_log.h"
#include "canvas.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "home_pages.h"
#include "sticky_app_lifecycle.h"
#include "sticky_battery.h"
#include "sticky_display.h"
#include "sticky_rtc.h"
#include "sticky_touch.h"

namespace {

constexpr char kTag[] = "home_app";
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(1000);
constexpr uint32_t kTaskStackSize = 4096U;
constexpr UBaseType_t kTaskPriority = 3U;
constexpr uint32_t kIdleSleepTimeoutMs = 10U * 60U * 1000U;
constexpr uint8_t kBatteryPollSeconds = 60U;

Canvas *s_canvas = nullptr;
TaskHandle_t s_task = nullptr;
StickyAppLifecycle s_lifecycle;
HomePageData s_page_data = {};
uint8_t s_battery_poll_countdown = 0U;

bool read_rtc()
{
    StickyRtcDateTime value = {};
    if (sticky_rtc_read(value) != ESP_OK) {
        s_page_data.rtc_valid = false;
        return false;
    }
    s_page_data.rtc_valid = true;
    s_page_data.year = value.year;
    s_page_data.month = value.month;
    s_page_data.day = value.day;
    s_page_data.hour = value.hour;
    s_page_data.minute = value.minute;
    return true;
}

void read_battery()
{
    StickyBatteryReading value = {};
    s_page_data.battery_valid =
        sticky_battery_read(value) == ESP_OK;
    if (s_page_data.battery_valid) {
        s_page_data.battery_percent = value.percent;
    }
    s_battery_poll_countdown = kBatteryPollSeconds;
}

esp_err_t render(bool partial)
{
    if (s_canvas == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    home_page_render(*s_canvas, s_page_data);
    return partial ? sticky_display_refresh_partial()
                   : sticky_display_refresh_monochrome();
}

void app_task(void *)
{
    sticky_touch_clear_press();
    read_rtc();
    read_battery();
    esp_err_t result = render(false);
    STICKY_LOGI(kTag,
                "home=ready rtc=%s battery=%s refresh=%s result=%s",
                s_page_data.rtc_valid ? "ready" : "unavailable",
                s_page_data.battery_valid ? "ready" : "unavailable",
                esp_err_to_name(result),
                result == ESP_OK ? "ok" : "failed");

    uint8_t rendered_minute = s_page_data.minute;
    uint8_t rendered_day = s_page_data.day;
    while (true) {
        if (sticky_app_lifecycle_checkpoint(s_lifecycle)) {
            sticky_touch_clear_press();
            read_rtc();
            read_battery();
            result = render(false);
            rendered_minute = s_page_data.minute;
            rendered_day = s_page_data.day;
            STICKY_LOGI(kTag,
                        "home=resume refresh=%s result=%s",
                        esp_err_to_name(result),
                        result == ESP_OK ? "ok" : "failed");
        }

        if (s_battery_poll_countdown > 0U) {
            --s_battery_poll_countdown;
        } else {
            read_battery();
        }

        if (read_rtc() &&
            (s_page_data.minute != rendered_minute ||
             s_page_data.day != rendered_day)) {
            result = render(true);
            rendered_minute = s_page_data.minute;
            rendered_day = s_page_data.day;
            STICKY_LOGI(kTag,
                        "home=clock minute=%u refresh=partial result=%s",
                        static_cast<unsigned>(rendered_minute),
                        esp_err_to_name(result));
        }
        vTaskDelay(kPollInterval);
    }
}

}  // namespace

esp_err_t home_app_start(Canvas &canvas)
{
    app_log_register_tag(kTag);
    if (s_task != nullptr) {
        return ESP_OK;
    }
    s_canvas = &canvas;
    if (xTaskCreate(app_task,
                    "home_app",
                    kTaskStackSize,
                    nullptr,
                    kTaskPriority,
                    &s_task) != pdPASS) {
        s_canvas = nullptr;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t home_app_pause()
{
    return sticky_app_lifecycle_pause(s_lifecycle, s_task);
}

esp_err_t home_app_resume()
{
    if (s_task == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    sticky_app_lifecycle_resume(s_lifecycle);
    return ESP_OK;
}

esp_err_t home_app_prepare_power_sleep(uint32_t &current_epoch_seconds,
                                       uint32_t &next_event_epoch_seconds)
{
    current_epoch_seconds = 0U;
    next_event_epoch_seconds = 0U;
    return home_app_pause();
}

uint32_t home_app_power_sleep_timeout_ms()
{
    return kIdleSleepTimeoutMs;
}

