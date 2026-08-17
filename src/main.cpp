#include "app_log.h"
#include "board_power.h"

#include <cinttypes>

#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef STICKY_FIRMWARE_VERSION
#define STICKY_FIRMWARE_VERSION "unknown"
#endif

#ifndef STICKY_BUILD_PROFILE
#define STICKY_BUILD_PROFILE "unknown"
#endif

namespace {

constexpr char kTag[] = "sticky_boot";
constexpr TickType_t kHeartbeatInterval = pdMS_TO_TICKS(30000);

const char *reset_reason_name(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_POWERON:
        return "power_on";
    case ESP_RST_EXT:
        return "external";
    case ESP_RST_SW:
        return "software";
    case ESP_RST_PANIC:
        return "panic";
    case ESP_RST_INT_WDT:
        return "interrupt_watchdog";
    case ESP_RST_TASK_WDT:
        return "task_watchdog";
    case ESP_RST_WDT:
        return "watchdog";
    case ESP_RST_DEEPSLEEP:
        return "deep_sleep";
    case ESP_RST_BROWNOUT:
        return "brownout";
    case ESP_RST_SDIO:
        return "sdio";
    case ESP_RST_UNKNOWN:
    default:
        return "unknown";
    }
}

const char *wakeup_reason_name(esp_sleep_wakeup_cause_t cause)
{
    switch (cause) {
    case ESP_SLEEP_WAKEUP_EXT0:
        return "ext0";
    case ESP_SLEEP_WAKEUP_EXT1:
        return "ext1";
    case ESP_SLEEP_WAKEUP_TIMER:
        return "timer";
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
        return "touchpad";
    case ESP_SLEEP_WAKEUP_ULP:
        return "ulp";
    case ESP_SLEEP_WAKEUP_GPIO:
        return "gpio";
    case ESP_SLEEP_WAKEUP_UART:
        return "uart";
    case ESP_SLEEP_WAKEUP_WIFI:
        return "wifi";
    case ESP_SLEEP_WAKEUP_COCPU:
        return "coprocessor";
    case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG:
        return "coprocessor_trap";
    case ESP_SLEEP_WAKEUP_UNDEFINED:
    case ESP_SLEEP_WAKEUP_ALL:
    default:
        return "undefined";
    }
}

#if STICKY_LOG_BOOT_DETAILS_ENABLED
void log_system_details()
{
    esp_chip_info_t chip = {};
    esp_chip_info(&chip);

    uint32_t flash_size = 0;
    const esp_err_t flash_result = esp_flash_get_size(nullptr, &flash_size);

    STICKY_LOGD(kTag,
                "system=idf version=%s chip_model=%d revision=%d cores=%d",
                esp_get_idf_version(),
                static_cast<int>(chip.model),
                static_cast<int>(chip.revision),
                static_cast<int>(chip.cores));

    if (flash_result == ESP_OK) {
        STICKY_LOGD(kTag,
                    "memory=flash size_bytes=%" PRIu32,
                    flash_size);
    } else {
        STICKY_LOGW(kTag,
                    "memory=flash result=%s",
                    esp_err_to_name(flash_result));
    }

    STICKY_LOGD(
        kTag,
        "memory=heap internal_free=%u internal_total=%u psram_free=%u psram_total=%u",
        static_cast<unsigned>(
            heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        static_cast<unsigned>(
            heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
        static_cast<unsigned>(heap_caps_get_total_size(MALLOC_CAP_SPIRAM)));
}
#endif

}  // namespace

extern "C" void app_main()
{
    app_log_init();
    app_log_register_tag(kTag);

    STICKY_LOGI(kTag,
                "phase=start firmware=%s profile=%s compile_level=%d",
                STICKY_FIRMWARE_VERSION,
                STICKY_BUILD_PROFILE,
                STICKY_LOG_COMPILE_LEVEL);
    STICKY_LOGI(kTag,
                "boot=reason reset=%s wake=%s",
                reset_reason_name(esp_reset_reason()),
                wakeup_reason_name(esp_sleep_get_wakeup_cause()));

    const esp_err_t power_result = board_power_init();
    if (power_result != ESP_OK) {
        STICKY_LOGE(kTag,
                    "phase=halt component=board_power result=%s",
                    esp_err_to_name(power_result));
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

#if STICKY_LOG_BOOT_DETAILS_ENABLED
    log_system_details();
#endif

    STICKY_LOGI(kTag, "phase=ready result=ok");

    while (true) {
#if STICKY_LOG_HEARTBEAT_ENABLED
        STICKY_LOGT(
            kTag,
            "heartbeat=alive uptime_ms=%" PRIu32 " internal_free=%u psram_free=%u",
            static_cast<uint32_t>(esp_log_timestamp()),
            static_cast<unsigned>(
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
            static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
#endif
        vTaskDelay(kHeartbeatInterval);
    }
}
