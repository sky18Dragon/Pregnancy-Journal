#include "board_power.h"

#include "app_log.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_config.h"

namespace {

constexpr char kTag[] = "sticky_power";
constexpr TickType_t kPowerSettleDelay = pdMS_TO_TICKS(100);

esp_err_t set_power_level(gpio_num_t pin, uint32_t level)
{
    const esp_err_t result = gpio_set_level(pin, level);
    if (result != ESP_OK) {
        STICKY_LOGE(kTag,
                    "step=set_level pin=%d level=%lu result=%s",
                    static_cast<int>(pin),
                    static_cast<unsigned long>(level),
                    esp_err_to_name(result));
    }
    return result;
}

}  // namespace

esp_err_t board_power_init()
{
    app_log_register_tag(kTag);

    const bool woke_from_sleep =
        esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED;
    STICKY_LOGI(kTag,
                "step=begin wake_from_sleep=%s hold_pin=%d lock_pin=%d",
                woke_from_sleep ? "true" : "false",
                PIN_POWER_HOLD,
                PIN_POWER_LOCK);

    if (woke_from_sleep) {
        // Preload retained outputs before releasing deep-sleep holds.
        // 在释放深度睡眠保持前预先恢复输出电平。
        gpio_set_direction(static_cast<gpio_num_t>(PIN_POWER_HOLD),
                           GPIO_MODE_OUTPUT);
        gpio_set_level(static_cast<gpio_num_t>(PIN_POWER_HOLD), 1);
        gpio_set_direction(static_cast<gpio_num_t>(PIN_POWER_LOCK),
                           GPIO_MODE_OUTPUT);
        gpio_set_level(static_cast<gpio_num_t>(PIN_POWER_LOCK), 1);
    }

    gpio_deep_sleep_hold_dis();
    gpio_hold_dis(static_cast<gpio_num_t>(PIN_POWER_HOLD));
    gpio_hold_dis(static_cast<gpio_num_t>(PIN_POWER_LOCK));

    gpio_config_t config = {};
    config.pin_bit_mask =
        (1ULL << PIN_POWER_HOLD) | (1ULL << PIN_POWER_LOCK);
    config.mode = GPIO_MODE_OUTPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;

    esp_err_t result = gpio_config(&config);
    if (result != ESP_OK) {
        STICKY_LOGE(kTag,
                    "step=configure_outputs result=%s",
                    esp_err_to_name(result));
        return result;
    }

    result = set_power_level(static_cast<gpio_num_t>(PIN_POWER_HOLD), 1);
    if (result != ESP_OK) {
        return result;
    }

    result = set_power_level(static_cast<gpio_num_t>(PIN_POWER_LOCK), 1);
    if (result != ESP_OK) {
        return result;
    }

    vTaskDelay(kPowerSettleDelay);
    STICKY_LOGI(kTag, "step=ready hold=high lock=high result=ok");
    return ESP_OK;
}

