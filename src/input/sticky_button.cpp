#include "sticky_button.h"

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
#ifndef STICKY_TOP_LONG_PRESS_MS
#define STICKY_TOP_LONG_PRESS_MS 2000
#endif

constexpr uint16_t kTopLongPressTimeMs = STICKY_TOP_LONG_PRESS_MS;
constexpr uint16_t kSideLongPressTimeMs = 2000U;
constexpr uint16_t kShortPressTimeMs = 180U;
constexpr UBaseType_t kEventQueueCapacity = 8U;
constexpr UBaseType_t kEdgeQueueCapacity = 16U;
constexpr uint8_t kReadyCapacity = 8U;

// A raw side-key edge, stamped when the callback ran.
struct SideEdge {
    SideKey side;
    bool pressed;
    int64_t at_us;
};

button_handle_t s_button = nullptr;
button_handle_t s_left_button = nullptr;
button_handle_t s_right_button = nullptr;
QueueHandle_t s_event_queue = nullptr;
QueueHandle_t s_edge_queue = nullptr;
// Only the consumer task touches the tracker and the ready ring.
SideKeyTracker s_tracker;
StickyButtonRecord s_ready[kReadyCapacity];
uint8_t s_ready_head = 0U;
uint8_t s_ready_count = 0U;

void queue_event(StickyButtonEvent event)
{
    const StickyButtonRecord record = {event, SideKey::Left,
                                       esp_timer_get_time()};
    xQueueSend(s_event_queue, &record, 0);
}

void push_ready(const StickyButtonRecord &record)
{
    if (s_ready_count >= kReadyCapacity) {
        return;
    }
    s_ready[(s_ready_head + s_ready_count) % kReadyCapacity] = record;
    ++s_ready_count;
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

void button_long_press_start_callback(void *button_handle, void *user_data)
{
    (void)button_handle;
    (void)user_data;
    queue_event(StickyButtonEvent::LongPressStart);
}

void button_long_press_up_callback(void *button_handle, void *user_data)
{
    (void)button_handle;
    (void)user_data;
    queue_event(StickyButtonEvent::LongPressEnd);
}

void side_button_callback(void *button_handle, void *user_data)
{
    const SideEdge edge = {
        button_handle == s_right_button ? SideKey::Right : SideKey::Left,
        reinterpret_cast<uintptr_t>(user_data) != 0U,
        esp_timer_get_time()};
    xQueueSend(s_edge_queue, &edge, 0);
}

esp_err_t create_side_button(int pin, button_handle_t &button)
{
    button_config_t button_config = {};
    button_config.long_press_time = kSideLongPressTimeMs;
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
                                     sizeof(StickyButtonRecord));
        if (s_event_queue == nullptr) {
            return ESP_ERR_NO_MEM;
        }
    }
    if (s_edge_queue == nullptr) {
        s_edge_queue = xQueueCreate(kEdgeQueueCapacity, sizeof(SideEdge));
        if (s_edge_queue == nullptr) {
            return ESP_ERR_NO_MEM;
        }
    }

    button_config_t button_config = {};
    button_config.long_press_time = kTopLongPressTimeMs;
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


    result = iot_button_register_cb(s_button,
                                    BUTTON_LONG_PRESS_START,
                                    nullptr,
                                    button_long_press_start_callback,
                                    nullptr);
    if (result != ESP_OK) {
        iot_button_delete(s_button);
        s_button = nullptr;
        return result;
    }

    result = iot_button_register_cb(s_button,
                                    BUTTON_LONG_PRESS_UP,
                                    nullptr,
                                    button_long_press_up_callback,
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
                "button=ready ai_pin=%d side_pins=%d,%d active_level=low press=imu_start long_start_ms=%u side_single=routed sleep_chord_ms=2000 click_window_ms=%u result=ok",
                PIN_TOP_BUTTON,
                PIN_SIDE_BUTTON_LEFT,
                PIN_SIDE_BUTTON_RIGHT,
                static_cast<unsigned>(kTopLongPressTimeMs),
                static_cast<unsigned>(kShortPressTimeMs));
    return ESP_OK;
}

bool sticky_button_take_record(StickyButtonRecord &record)
{
    if (s_event_queue == nullptr || s_edge_queue == nullptr) {
        return false;
    }

    SideEdge edge;
    while (xQueueReceive(s_edge_queue, &edge, 0) == pdTRUE) {
        SideKeyEvent single;
        if (s_tracker.on_edge(edge.side, edge.pressed, edge.at_us, single)) {
            push_ready({StickyButtonEvent::SideSingle, single.side,
                        single.at_us});
        }
    }
    SideKeyEvent chord;
    if (s_tracker.poll(esp_timer_get_time(), chord)) {
        STICKY_LOGI(kTag,
                    "button=sleep_chord state=detected hold_ms=2000 result=ok");
        push_ready({StickyButtonEvent::SleepChord, SideKey::Left, chord.at_us});
    }

    if (s_ready_count > 0U) {
        record = s_ready[s_ready_head];
        s_ready_head = (s_ready_head + 1U) % kReadyCapacity;
        --s_ready_count;
        return true;
    }
    record = StickyButtonRecord();
    return xQueueReceive(s_event_queue, &record, 0) == pdTRUE;
}

bool sticky_button_take_event(StickyButtonEvent &event)
{
    StickyButtonRecord record;
    while (sticky_button_take_record(record)) {
        if (record.event != StickyButtonEvent::SideSingle) {
            event = record.event;
            return true;
        }
    }
    event = StickyButtonEvent::None;
    return false;
}
