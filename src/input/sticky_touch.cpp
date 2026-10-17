#include "sticky_touch.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>

#include "app_log.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "gt911.h"
#include "pin_config.h"
#include "sticky_touch_recovery_policy.h"

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
constexpr UBaseType_t kInteractionQueueLength = 8;
constexpr TickType_t kStopTimeout = pdMS_TO_TICKS(1000);
constexpr uint16_t kTapMoveTolerance = 24U;
constexpr uint32_t kRecoveryRetryDelayMs = 1000U;
constexpr uint32_t kPowerOffHoldMs = 300U;   // touch rail off long enough for the controller to drain
constexpr uint32_t kPowerOnSettleMs = 250U;  // same settle time as the cold-start path in sticky_touch_init()

i2c_master_bus_handle_t s_touch_bus = nullptr;
GT911 s_controller;
TaskHandle_t s_touch_task = nullptr;
bool s_touching = false;
bool s_read_error_reported = false;
uint32_t s_consecutive_read_failures = 0U;
uint32_t s_next_recovery_attempt_ms = 0U;
bool s_geometry_valid = true;       // the controller reported the verified geometry at its last start
bool s_touch_seen = false;          // a contact has been read since boot
uint32_t s_geometry_retries = 0U;
uint32_t s_next_geometry_retry_ms = 0U;
uint16_t s_last_x = 0;
uint16_t s_last_y = 0;
uint16_t s_start_x = 0;
uint16_t s_start_y = 0;
uint32_t s_started_at_ms = 0U;
QueueHandle_t s_press_queue = nullptr;
QueueHandle_t s_interaction_queue = nullptr;
std::atomic<bool> s_stop_requested{false};
std::atomic<uint32_t> s_last_activity_ms{0U};

uint32_t monotonic_ms()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000LL);
}

bool time_reached(uint32_t now_ms, uint32_t target_ms)
{
    return static_cast<int32_t>(now_ms - target_ms) >= 0;
}

// Cuts the touch rail and brings it back, reproducing the cold start that normally yields a healthy
// controller. A reset-pin pulse alone does not free a controller that powered up without loading its
// configuration (it keeps reporting 2048x2048 or zeros), a real power cycle does.
// 断开再接通触摸电源，复现正常的冷启动。仅拉复位脚救不回上电后没有加载配置的控制器，真正掉电重启可以。
void power_cycle_touch_controller()
{
    gpio_set_direction(static_cast<gpio_num_t>(PIN_TOUCH_RST), GPIO_MODE_OUTPUT);
    gpio_set_direction(static_cast<gpio_num_t>(PIN_TOUCH_INT), GPIO_MODE_OUTPUT);
    gpio_set_level(static_cast<gpio_num_t>(PIN_TOUCH_RST), 0);
    gpio_set_level(static_cast<gpio_num_t>(PIN_TOUCH_INT), 0);
    gpio_set_level(static_cast<gpio_num_t>(PIN_TOUCH_EN), 0);
    vTaskDelay(pdMS_TO_TICKS(kPowerOffHoldMs));
    gpio_set_level(static_cast<gpio_num_t>(PIN_TOUCH_EN), 1);
    vTaskDelay(pdMS_TO_TICKS(kPowerOnSettleMs));
}

// Starts GT911 with the same reset and address-selection flow as the Sticky demo. When the
// controller reports an unusable geometry, the whole start is repeated after a power cycle of the
// touch rail (`power_cycle_first` does that before the very first attempt as well, for retries
// that already know a plain reset did not help).
// 使用Sticky示例相同的复位和地址选择流程启动GT911；报告的分辨率不可用时，掉电重启触摸电源后整体重来。
bool start_touch_controller(const char *reason, bool power_cycle_first = false)
{
    if (power_cycle_first) {
        power_cycle_touch_controller();
    }
    // A controller that comes up reporting a wrong geometry delivers no touches, so repeat the
    // reset flow a few times before settling for the fallback mapping.
    // 控制器启动后若报告错误分辨率就收不到触摸，所以先重复几次复位流程，再退回兜底映射。
    uint16_t reported_width = 0U;
    uint16_t reported_height = 0U;
    bool resolution_read = false;
    for (uint32_t attempt = 1U; attempt <= kStickyTouchInitAttempts; ++attempt) {
        if (!s_controller.begin(PIN_TOUCH_INT,
                                PIN_TOUCH_RST,
                                kDisplayWidth,
                                kDisplayHeight,
                                s_touch_bus)) {
            STICKY_LOGW(kTag,
                        "touch=controller_start reason=%s attempt=%u result=failed",
                        reason,
                        static_cast<unsigned>(attempt));
            return false;
        }

        reported_width = 0U;
        reported_height = 0U;
        resolution_read =
            s_controller.readResolution(reported_width, reported_height);
        if (sticky_touch_geometry_valid(resolution_read, reported_width, reported_height)) {
            break;
        }
        STICKY_LOGW(kTag,
                    "touch=geometry reason=%s attempt=%u of=%u read_ok=%u reported_width=%u reported_height=%u result=invalid",
                    reason,
                    static_cast<unsigned>(attempt),
                    static_cast<unsigned>(kStickyTouchInitAttempts),
                    static_cast<unsigned>(resolution_read),
                    static_cast<unsigned>(reported_width),
                    static_cast<unsigned>(reported_height));
        if (attempt < kStickyTouchInitAttempts) {
            power_cycle_touch_controller();
        }
    }
    const StickyTouchSensorResolution resolution =
        sticky_touch_select_sensor_resolution(
            resolution_read, reported_width, reported_height);

    // The reference hardware reports 480x800. The fallback changes only the
    // software mapping and leaves the controller configuration untouched.
    // 示例硬件分辨率为480x800；兜底只调整软件映射，不修改控制器配置。
    s_controller.setSensorResolution(resolution.width, resolution.height);
    s_consecutive_read_failures = 0U;
    s_read_error_reported = false;
    s_next_recovery_attempt_ms = 0U;
    s_geometry_valid = !resolution.used_fallback;
    if (s_geometry_valid) {
        s_geometry_retries = 0U;
    }

    STICKY_LOGI(kTag,
                "touch=controller_ready reason=%s address=0x%02X reported_width=%u reported_height=%u map_width=%u map_height=%u fallback=%u result=ok",
                reason,
                s_controller.address(),
                static_cast<unsigned>(reported_width),
                static_cast<unsigned>(reported_height),
                static_cast<unsigned>(resolution.width),
                static_cast<unsigned>(resolution.height),
                static_cast<unsigned>(resolution.used_fallback));
    return true;
}

// Repeats the reference reset flow after persistent polling faults.
// 连续轮询异常后重新执行示例复位流程。
bool recover_touch_controller(const char *reason, uint32_t now_ms)
{
    if (s_next_recovery_attempt_ms != 0U &&
        !time_reached(now_ms, s_next_recovery_attempt_ms)) {
        return false;
    }

    STICKY_LOGW(kTag,
                "touch=recovery reason=%s failures=%u state=started",
                reason,
                static_cast<unsigned>(s_consecutive_read_failures));
    s_touching = false;
    const bool geometry_retry = std::strcmp(reason, "invalid_geometry") == 0;
    if (start_touch_controller(reason, geometry_retry)) {
        STICKY_LOGI(kTag,
                    "touch=recovery reason=%s state=finished result=ok",
                    reason);
        return true;
    }

    s_next_recovery_attempt_ms = now_ms + kRecoveryRetryDelayMs;
    STICKY_LOGW(kTag,
                "touch=recovery reason=%s retry_ms=%u state=finished result=failed",
                reason,
                static_cast<unsigned>(kRecoveryRetryDelayMs));
    return false;
}

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

uint16_t coordinate_distance(uint16_t first, uint16_t second)
{
    return first >= second ? first - second : second - first;
}

// Completes one path and emits a tap only when the finger stayed in place.
// 完成一条触摸轨迹；手指基本停留在原位时才额外生成点击事件。
void finish_touch_interaction(uint32_t ended_at_ms)
{
    const StickyTouchInteraction interaction = {
        s_start_x,
        s_start_y,
        s_last_x,
        s_last_y,
        s_started_at_ms,
        ended_at_ms,
    };
    if (xQueueSend(s_interaction_queue, &interaction, 0) != pdTRUE) {
        STICKY_LOGW(kTag,
                    "touch=interaction_queue result=full capacity=%u",
                    static_cast<unsigned>(kInteractionQueueLength));
    }

    const uint16_t distance_x = coordinate_distance(s_start_x, s_last_x);
    const uint16_t distance_y = coordinate_distance(s_start_y, s_last_y);
    if (distance_x > kTapMoveTolerance ||
        distance_y > kTapMoveTolerance) {
        STICKY_LOGD(kTag,
                    "touch=path start_x=%u start_y=%u end_x=%u end_y=%u duration_ms=%u result=gesture",
                    static_cast<unsigned>(s_start_x),
                    static_cast<unsigned>(s_start_y),
                    static_cast<unsigned>(s_last_x),
                    static_cast<unsigned>(s_last_y),
                    static_cast<unsigned>(ended_at_ms - s_started_at_ms));
        return;
    }

    const StickyTouchPress press = {
        s_last_x,
        s_last_y,
        ended_at_ms,
    };
    if (xQueueSend(s_press_queue, &press, 0) == pdTRUE) {
        STICKY_LOGI(kTag,
                    "touch=detected x=%u y=%u queued=%u",
                    static_cast<unsigned>(s_last_x),
                    static_cast<unsigned>(s_last_y),
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

void touch_task(void *)
{
    TickType_t next_poll = xTaskGetTickCount();
    while (!s_stop_requested.load(std::memory_order_acquire)) {
        GTPoint point = {};
        const int8_t count = s_controller.read_points(&point, 1);
        const uint32_t now_ms = monotonic_ms();
        if (count > 0) {
            s_touch_seen = true;
            s_consecutive_read_failures = 0U;
            s_read_error_reported = false;
            if (s_touching &&
                sticky_touch_contact_stuck(s_started_at_ms, now_ms)) {
                (void)recover_touch_controller("contact_stuck", now_ms);
                vTaskDelayUntil(&next_poll, kPollInterval);
                continue;
            }

            transform_touch_coordinate(point.x,
                                       point.y,
                                       s_last_x,
                                       s_last_y);
            if (!s_touching) {
                s_touching = true;
                s_start_x = s_last_x;
                s_start_y = s_last_y;
                s_started_at_ms = now_ms;
                s_last_activity_ms.store(now_ms,
                                         std::memory_order_release);
            }
        } else if (count == 0) {
            s_consecutive_read_failures = 0U;
            s_read_error_reported = false;
            if (s_touching) {
                s_last_activity_ms.store(now_ms,
                                         std::memory_order_release);
                finish_touch_interaction(now_ms);
                s_touching = false;
            }
        } else if (count < 0) {
            s_touching = false;
            ++s_consecutive_read_failures;
            if (!s_read_error_reported) {
                STICKY_LOGW(kTag,
                            "touch=read failures=%u result=failed",
                            static_cast<unsigned>(s_consecutive_read_failures));
                s_read_error_reported = true;
            }
            if (sticky_touch_recovery_required(
                    s_consecutive_read_failures)) {
                (void)recover_touch_controller("read_failures", now_ms);
            }
        }

        // A controller that came up with a wrong geometry and has never delivered a touch is dead
        // for the user: re-run the reset flow now and then, backing off while it stays broken.
        // 启动时分辨率无效且从未收到过触摸，用户眼里触摸已失效：隔一段时间重新执行复位流程，持续失败则退避。
        if (sticky_touch_geometry_retry_due(s_geometry_valid, s_touch_seen, s_touching, now_ms,
                                            s_next_geometry_retry_ms)) {
            s_next_geometry_retry_ms =
                now_ms + sticky_touch_geometry_retry_delay_ms(s_geometry_retries);
            ++s_geometry_retries;
            (void)recover_touch_controller("invalid_geometry", now_ms);
            if (s_geometry_valid) {
                STICKY_LOGI(kTag, "touch=geometry result=recovered retries=%u",
                            static_cast<unsigned>(s_geometry_retries));
            }
        }

        vTaskDelayUntil(&next_poll, kPollInterval);
    }
    s_touch_task = nullptr;
    vTaskDelete(nullptr);
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

    if (!start_touch_controller("boot")) {
        return ESP_FAIL;
    }
    s_touch_seen = false;
    s_geometry_retries = 0U;
    s_next_geometry_retry_ms = monotonic_ms() + kStickyTouchGeometryRetryFirstMs;

    // Stores up to eight ordered press events in a FreeRTOS queue.
    // 使用FreeRTOS队列按顺序保存最多8个按下事件。
    s_press_queue = xQueueCreate(kPressQueueLength, sizeof(StickyTouchPress));
    if (s_press_queue == nullptr) {
        return ESP_ERR_NO_MEM;
    }
    s_interaction_queue = xQueueCreate(
        kInteractionQueueLength, sizeof(StickyTouchInteraction));
    if (s_interaction_queue == nullptr) {
        vQueueDelete(s_press_queue);
        s_press_queue = nullptr;
        return ESP_ERR_NO_MEM;
    }

#if !STICKY_LOG_TOUCH_DRIVER_OUTPUT_ENABLED
    // Keeps the reference driver code enabled while silencing its polling logs.
    // 保留参考驱动的完整执行路径，同时关闭轮询日志输出。
    esp_log_level_set("GT911", ESP_LOG_ERROR);
#endif

    s_stop_requested.store(false, std::memory_order_release);
    if (xTaskCreate(touch_task,
                    "sticky_touch",
                    kTaskStackSize,
                    nullptr,
                    kTaskPriority,
                    &s_touch_task) != pdPASS) {
        vQueueDelete(s_press_queue);
        s_press_queue = nullptr;
        vQueueDelete(s_interaction_queue);
        s_interaction_queue = nullptr;
        return ESP_ERR_NO_MEM;
    }
    STICKY_LOGI(kTag,
                "touch=polling_ready interval_ms=30 tap_queue_capacity=%u interaction_queue_capacity=%u driver_output=%d result=ok",
                static_cast<unsigned>(kPressQueueLength),
                static_cast<unsigned>(kInteractionQueueLength),
                STICKY_LOG_TOUCH_DRIVER_OUTPUT_ENABLED);
    return ESP_OK;
}

esp_err_t sticky_touch_stop()
{
    if (s_touch_task == nullptr) {
        return ESP_OK;
    }

    s_stop_requested.store(true, std::memory_order_release);
    const TickType_t deadline = xTaskGetTickCount() + kStopTimeout;
    while (s_touch_task != nullptr) {
        if (static_cast<int32_t>(xTaskGetTickCount() - deadline) >= 0) {
            STICKY_LOGE(kTag, "touch=stop result=timeout");
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    sticky_touch_clear_press();
    sticky_touch_clear_interaction();
    s_touching = false;
    STICKY_LOGI(kTag, "touch=monitoring state=stopped result=ok");
    return ESP_OK;
}

bool sticky_touch_take_press(StickyTouchPress &press)
{
    return s_press_queue != nullptr &&
           xQueueReceive(s_press_queue, &press, 0) == pdTRUE;
}

bool sticky_touch_take_interaction(StickyTouchInteraction &interaction)
{
    return s_interaction_queue != nullptr &&
           xQueueReceive(s_interaction_queue, &interaction, 0) == pdTRUE;
}

void sticky_touch_clear_press()
{
    if (s_press_queue != nullptr) {
        xQueueReset(s_press_queue);
    }
}

void sticky_touch_clear_interaction()
{
    if (s_interaction_queue != nullptr) {
        xQueueReset(s_interaction_queue);
    }
}

uint32_t sticky_touch_last_activity_ms()
{
    return s_last_activity_ms.load(std::memory_order_acquire);
}
