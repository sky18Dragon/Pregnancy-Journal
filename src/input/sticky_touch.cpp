#include "sticky_touch.h"

#include <algorithm>
#include <cstdint>

#include "app_log.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "gt911.h"
#include "pin_config.h"

namespace {

constexpr char kTag[] = "sticky_touch";
constexpr uint16_t kDisplayWidth = 800;
constexpr uint16_t kDisplayHeight = 480;
constexpr uint16_t kPortraitWidth = 480;
constexpr uint16_t kPortraitHeight = 800;
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(30);
constexpr uint32_t kTaskStackSize = 4096;
constexpr UBaseType_t kTaskPriority = 5;
constexpr UBaseType_t kPressQueueLength = 8;

i2c_master_bus_handle_t s_touch_bus = nullptr;
GT911 s_controller;
TaskHandle_t s_touch_task = nullptr;
bool s_touching = false;
bool s_read_error_reported = false;
uint16_t s_last_x = 0;
uint16_t s_last_y = 0;
QueueHandle_t s_press_queue = nullptr;

uint16_t scale_coordinate(uint16_t value,
                          uint16_t source_max,
                          uint16_t target_max)
{
    const uint16_t clamped = std::min(value, source_max);
    return static_cast<uint16_t>(
        (static_cast<uint32_t>(clamped) * target_max + source_max / 2U) /
        source_max);
}

void transform_touch_coordinate(uint16_t controller_x,
                                uint16_t controller_y,
                                uint16_t &screen_x,
                                uint16_t &screen_y)
{
    // Converts the portrait-oriented touch axes into physical screen axes.
    // 将竖向排列的触摸坐标转换为屏幕的物理坐标。
    const uint16_t portrait_x =
        scale_coordinate(controller_x, kDisplayWidth, kPortraitWidth - 1U);
    const uint16_t clamped_y = std::min(controller_y, kDisplayHeight);
    const uint16_t portrait_y =
        scale_coordinate(kDisplayHeight - clamped_y,
                         kDisplayHeight,
                         kPortraitHeight - 1U);

    const uint16_t framebuffer_x = kDisplayWidth - portrait_y - 1U;
    const uint16_t framebuffer_y = portrait_x;

    // The display layer transmits a 180-degree rotated framebuffer.
    // 显示层会把画布旋转180度后再发送到屏幕。
    screen_x = kDisplayWidth - framebuffer_x - 1U;
    screen_y = kDisplayHeight - framebuffer_y - 1U;
}

void touch_task(void *)
{
    TickType_t next_poll = xTaskGetTickCount();
    while (true) {
        GTPoint point = {};
        const int8_t count = s_controller.read_points(&point, 1);
        if (count > 0) {
            transform_touch_coordinate(point.x,
                                       point.y,
                                       s_last_x,
                                       s_last_y);
            if (!s_touching) {
                s_touching = true;
                const StickyTouchPress press = {
                    s_last_x,
                    s_last_y,
                    static_cast<uint32_t>(esp_timer_get_time() / 1000LL),
                };
                if (xQueueSend(s_press_queue, &press, 0) == pdTRUE) {
                    STICKY_LOGI(kTag,
                                "touch=detected x=%u y=%u id=%u size=%u queued=%u",
                                static_cast<unsigned>(s_last_x),
                                static_cast<unsigned>(s_last_y),
                                static_cast<unsigned>(point.id),
                                static_cast<unsigned>(point.size),
                                static_cast<unsigned>(
                                    uxQueueMessagesWaiting(s_press_queue)));
                } else {
                    STICKY_LOGW(kTag,
                                "touch=queue result=full capacity=%u x=%u y=%u",
                                static_cast<unsigned>(kPressQueueLength),
                                static_cast<unsigned>(s_last_x),
                                static_cast<unsigned>(s_last_y));
                }
            }
            s_read_error_reported = false;
        } else if (count == 0) {
            s_touching = false;
        } else if (count < 0) {
            s_touching = false;
            if (!s_read_error_reported) {
                STICKY_LOGW(kTag, "touch=read result=failed");
                s_read_error_reported = true;
            }
        }

        vTaskDelayUntil(&next_poll, kPollInterval);
    }
}

}  // namespace

esp_err_t sticky_touch_init()
{
    app_log_register_tag(kTag);
    app_log_register_tag("GT911");
    STICKY_LOGI(kTag,
                "touch=init_begin sda=%d scl=%d int=%d reset=%d enable=%d",
                PIN_TOUCH_SDA,
                PIN_TOUCH_SCL,
                PIN_TOUCH_INT,
                PIN_TOUCH_RST,
                PIN_TOUCH_EN);

    gpio_config_t power_config = {};
    power_config.pin_bit_mask = 1ULL << PIN_TOUCH_EN;
    power_config.mode = GPIO_MODE_OUTPUT;
    power_config.pull_up_en = GPIO_PULLUP_DISABLE;
    power_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    power_config.intr_type = GPIO_INTR_DISABLE;
    esp_err_t result = gpio_config(&power_config);
    if (result != ESP_OK) {
        return result;
    }
    gpio_hold_dis(static_cast<gpio_num_t>(PIN_TOUCH_EN));
    result = gpio_set_level(static_cast<gpio_num_t>(PIN_TOUCH_EN), 1);
    if (result != ESP_OK) {
        return result;
    }
    vTaskDelay(pdMS_TO_TICKS(250));

    i2c_master_bus_config_t bus_config = {};
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.sda_io_num = static_cast<gpio_num_t>(PIN_TOUCH_SDA);
    bus_config.scl_io_num = static_cast<gpio_num_t>(PIN_TOUCH_SCL);
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = 1;
    result = i2c_new_master_bus(&bus_config, &s_touch_bus);
    if (result != ESP_OK) {
        return result;
    }

    if (!s_controller.begin(PIN_TOUCH_INT,
                            PIN_TOUCH_RST,
                            kDisplayWidth,
                            kDisplayHeight,
                            s_touch_bus)) {
        return ESP_FAIL;
    }
    uint16_t sensor_width = 0;
    uint16_t sensor_height = 0;
    s_controller.readResolution(sensor_width, sensor_height);
    STICKY_LOGI(kTag,
                "touch=controller_ready address=0x%02X sensor_width=%u sensor_height=%u result=ok",
                s_controller.address(),
                static_cast<unsigned>(sensor_width),
                static_cast<unsigned>(sensor_height));

    // Stores up to eight ordered press events in a FreeRTOS queue.
    // 使用FreeRTOS队列按顺序保存最多8个按下事件。
    s_press_queue = xQueueCreate(kPressQueueLength, sizeof(StickyTouchPress));
    if (s_press_queue == nullptr) {
        return ESP_ERR_NO_MEM;
    }

#if !STICKY_LOG_TOUCH_DRIVER_OUTPUT_ENABLED
    // Keeps the reference driver code enabled while silencing its polling logs.
    // 保留参考驱动的完整执行路径，同时关闭轮询日志输出。
    esp_log_level_set("GT911", ESP_LOG_ERROR);
#endif

    if (xTaskCreate(touch_task,
                    "sticky_touch",
                    kTaskStackSize,
                    nullptr,
                    kTaskPriority,
                    &s_touch_task) != pdPASS) {
        vQueueDelete(s_press_queue);
        s_press_queue = nullptr;
        return ESP_ERR_NO_MEM;
    }
    STICKY_LOGI(kTag,
                "touch=polling_ready interval_ms=30 queue_capacity=%u driver_output=%d result=ok",
                static_cast<unsigned>(kPressQueueLength),
                STICKY_LOG_TOUCH_DRIVER_OUTPUT_ENABLED);
    return ESP_OK;
}

bool sticky_touch_take_press(StickyTouchPress &press)
{
    return s_press_queue != nullptr &&
           xQueueReceive(s_press_queue, &press, 0) == pdTRUE;
}

void sticky_touch_clear_press()
{
    if (s_press_queue != nullptr) {
        xQueueReset(s_press_queue);
    }
}
