#include "sticky_button.h"

#include <atomic>
#include <cstdint>

#include "app_log.h"
#include "button_gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "iot_button.h"
#include "pin_config.h"
#include "esp_timer.h"

namespace {

constexpr char kTag[] = "sticky_button";
constexpr uint16_t kLongPressTimeMs = 2000U;
constexpr uint16_t kShortPressTimeMs = 180U;
constexpr UBaseType_t kEventQueueCapacity = 4U;
constexpr int64_t kSleepChordHoldUs = 2000000LL;
constexpr int64_t kSleepChordPressWindowUs = 500000LL;

button_handle_t s_button = nullptr;
button_handle_t s_left_button = nullptr;
button_handle_t s_right_button = nullptr;
QueueHandle_t s_event_queue = nullptr;
std::atomic<bool> s_left_pressed{false};
std::atomic<bool> s_right_pressed{false};
std::atomic<int64_t> s_left_pressed_at_us{0};
std::atomic<int64_t> s_right_pressed_at_us{0};
bool s_sleep_chord_emitted = false;

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

void side_button_callback(void *button_handle, void *user_data)
{
    const bool pressed = reinterpret_cast<uintptr_t>(user_data) != 0U;
    const int64_t now_us = esp_timer_get_time();
    if (button_handle == s_left_button) {
        s_left_pressed.store(pressed, std::memory_order_release);
        s_left_pressed_at_us.store(pressed ? now_us : 0,
                                   std::memory_order_release);
    } else if (button_handle == s_right_button) {
        s_right_pressed.store(pressed, std::memory_order_release);
        s_right_pressed_at_us.store(pressed ? now_us : 0,
                                    std::memory_order_release);
    }
}

esp_err_t create_side_button(int pin, button_handle_t &button)
{
    button_config_t button_config = {};
    button_config.long_press_time = kLongPressTimeMs;
    button_config.short_press_time = kShortPressTimeMs;

    button_gpio_config_t gpio_config = {};
    gpio_config.gpio_num = pin;
    gpio_config.active_level = 0;
    gpio_config.enable_power_save = false;
    gpio_config.disable_pull = false;

    esp_err_t result = iot_button_new_gpio_device(
        &button_config, &gpio_config, &button);
    if (result != ESP_OK) {
        return result;
    }
    result = iot_button_register_cb(
        button,
        BUTTON_PRESS_DOWN,
        nullptr,
        side_button_callback,
        reinterpret_cast<void *>(static_cast<uintptr_t>(1U)));
    if (result != ESP_OK) {
        return result;
    }
    return iot_button_register_cb(
        button,
        BUTTON_PRESS_UP,
        nullptr,
        side_button_callback,
        nullptr);
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


    result = create_side_button(PIN_SIDE_BUTTON_LEFT, s_left_button);
    if (result != ESP_OK) {
        return result;
    }
    result = create_side_button(PIN_SIDE_BUTTON_RIGHT, s_right_button);
    if (result != ESP_OK) {
        return result;
    }

    STICKY_LOGI(kTag,
                "button=ready ai_pin=%d side_pins=%d,%d active_level=low press=imu_start single=launcher double=pregnancy_home sleep_chord_ms=2000 click_window_ms=%u result=ok",
                PIN_TOP_BUTTON,
                PIN_SIDE_BUTTON_LEFT,
                PIN_SIDE_BUTTON_RIGHT,
                static_cast<unsigned>(kShortPressTimeMs));
    return ESP_OK;
}

bool sticky_button_take_event(StickyButtonEvent &event)
{
    const bool left_pressed =
        s_left_pressed.load(std::memory_order_acquire);
    const bool right_pressed =
        s_right_pressed.load(std::memory_order_acquire);
    if (!left_pressed || !right_pressed) {
        s_sleep_chord_emitted = false;
    } else if (!s_sleep_chord_emitted) {
        const int64_t left_at =
            s_left_pressed_at_us.load(std::memory_order_acquire);
        const int64_t right_at =
            s_right_pressed_at_us.load(std::memory_order_acquire);
        const int64_t press_gap_us = left_at > right_at
                                         ? left_at - right_at
                                         : right_at - left_at;
        const int64_t held_since_us = left_at > right_at
                                          ? left_at
                                          : right_at;
        if (press_gap_us <= kSleepChordPressWindowUs &&
            esp_timer_get_time() - held_since_us >= kSleepChordHoldUs) {
            s_sleep_chord_emitted = true;
            event = StickyButtonEvent::SleepChord;
            STICKY_LOGI(kTag,
                        "button=sleep_chord state=detected hold_ms=2000 result=ok");
            return true;
        }
    }

    if (s_event_queue == nullptr) {
        return false;
    }
    event = StickyButtonEvent::None;
    return xQueueReceive(s_event_queue, &event, 0) == pdTRUE;
}
