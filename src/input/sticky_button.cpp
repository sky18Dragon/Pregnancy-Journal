#include "sticky_button.h"

#include <cstdint>

#include "app_log.h"
#include "button_gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "iot_button.h"
#include "pin_config.h"

namespace {

constexpr char kTag[] = "sticky_button";
constexpr uint16_t kLongPressTimeMs = 2000U;
constexpr uint16_t kShortPressTimeMs = 180U;
constexpr UBaseType_t kEventQueueCapacity = 4U;

button_handle_t s_button = nullptr;
QueueHandle_t s_event_queue = nullptr;

void queue_event(StickyButtonEvent event)
{
    xQueueSend(s_event_queue, &event, 0);
}

void button_press_down_callback(void *button_handle, void *user_data)
{
    (void)button_handle;
    (void)user_data;
    queue_event(StickyButtonEvent::PressDown);
}

void button_single_click_callback(void *button_handle, void *user_data)
{
    (void)button_handle;
    (void)user_data;
    queue_event(StickyButtonEvent::SingleClick);
}

void button_double_click_callback(void *button_handle, void *user_data)
{
    (void)button_handle;
    (void)user_data;
    queue_event(StickyButtonEvent::DoubleClick);
}

}  // namespace

esp_err_t sticky_button_init()
{
    app_log_register_tag(kTag);
    if (s_button != nullptr) {
        return ESP_OK;
    }

    if (s_event_queue == nullptr) {
        s_event_queue = xQueueCreate(kEventQueueCapacity,
                                     sizeof(StickyButtonEvent));
        if (s_event_queue == nullptr) {
            return ESP_ERR_NO_MEM;
        }
    }

    button_config_t button_config = {};
    button_config.long_press_time = kLongPressTimeMs;
    button_config.short_press_time = kShortPressTimeMs;

    button_gpio_config_t gpio_config = {};
    gpio_config.gpio_num = PIN_TOP_BUTTON;
    gpio_config.active_level = 0;
    gpio_config.enable_power_save = false;
    gpio_config.disable_pull = false;

    esp_err_t result = iot_button_new_gpio_device(
        &button_config, &gpio_config, &s_button);
    if (result != ESP_OK) {
        s_button = nullptr;
        return result;
    }

    result = iot_button_register_cb(s_button,
                                    BUTTON_PRESS_DOWN,
                                    nullptr,
                                    button_press_down_callback,
                                    nullptr);
    if (result != ESP_OK) {
        iot_button_delete(s_button);
        s_button = nullptr;
        return result;
    }

    result = iot_button_register_cb(s_button,
                                    BUTTON_SINGLE_CLICK,
                                    nullptr,
                                    button_single_click_callback,
                                    nullptr);
    if (result != ESP_OK) {
        iot_button_delete(s_button);
        s_button = nullptr;
        return result;
    }

    result = iot_button_register_cb(s_button,
                                    BUTTON_DOUBLE_CLICK,
                                    nullptr,
                                    button_double_click_callback,
                                    nullptr);
    if (result != ESP_OK) {
        iot_button_delete(s_button);
        s_button = nullptr;
        return result;
    }

    STICKY_LOGI(kTag,
                "button=ready pin=%d active_level=low press=imu_start single=launcher double=pet_home click_window_ms=%u result=ok",
                PIN_TOP_BUTTON,
                static_cast<unsigned>(kShortPressTimeMs));
    return ESP_OK;
}

bool sticky_button_take_event(StickyButtonEvent &event)
{
    if (s_event_queue == nullptr) {
        return false;
    }
    event = StickyButtonEvent::None;
    return xQueueReceive(s_event_queue, &event, 0) == pdTRUE;
}
