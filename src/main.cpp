#include "app_log.h"
#include "board_charger.h"
#include "board_power.h"
#include "board_sensor_bus.h"
#include "board_shared_spi.h"
#include "sticky_sd_audio_test.h"
#include "canvas.h"
#include "domain_repository_stores.h"
#include "sticky_app.h"
#include "sticky_battery.h"
#include "sticky_buzzer.h"
#include "sticky_display.h"
#include "sticky_imu.h"
#include "sticky_rtc.h"
#include "sd_storage_service.h"
#include "sticky_manifest_commit_test.h"
#include "sticky_journal_power_test.h"
#include "sticky_object_power_test.h"
#include "sticky_index_power_test.h"
#include "sticky_checkpoint_commit_test.h"
#include "sticky_storage_status_test.h"
#include "sticky_touch.h"
#include "sticky_voice_classifier_test.h"
#include "font_test_screen.h"
#include "sticky_chinese_input_test.h"
#include "ui_language.h"

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
#include "nvs_flash.h"

#ifndef STICKY_FIRMWARE_VERSION
#define STICKY_FIRMWARE_VERSION "unknown"
#endif

#ifndef STICKY_BUILD_PROFILE
#define STICKY_BUILD_PROFILE "unknown"
#endif

#ifndef UI_DEBUG_FONT_TEST
#define UI_DEBUG_FONT_TEST 0
#endif

#ifndef UI_DEBUG_FONT_TEST_PORTRAIT
#define UI_DEBUG_FONT_TEST_PORTRAIT 0
#endif

namespace {

constexpr char kTag[] = "sticky_boot";
constexpr TickType_t kHeartbeatInterval = pdMS_TO_TICKS(30000);

// Keeps the device alive after a required startup component fails.
// 必要组件启动失败后保持设备运行，方便串口持续保留最后一条错误信息。
[[noreturn]] void halt_after_error(const char *component, esp_err_t result)
{
    STICKY_LOGE(kTag,
                "phase=halt component=%s result=%s",
                component,
                esp_err_to_name(result));
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

const char *reset_reason_name(esp_reset_reason_t reason)
{
    // Convert hardware reset codes into stable text fields for log comparison.
    // 将硬件复位代码转换成稳定文本，方便对比不同测试日志。
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
    // Deep-sleep wake sources use a separate ESP-IDF enumeration from reset reasons.
    // 深度睡眠唤醒原因使用另一套枚举，因此在这里单独转换。
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
    // These diagnostics are compiled only into profiles that request boot details.
    // 这些硬件诊断只会编译进启用了启动详情的固件配置。
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

bool load_device_preferences_from_sd()
{
    using sticky_storage::SdRepositoryAdapterStatus;
    auto &store = pregnancy_repository::device_preferences_repository_store();
    constexpr unsigned kMaximumPolls = 125U;
    for (unsigned poll = 0U; poll < kMaximumPolls; ++poll) {
        SdRepositoryAdapterStatus status = store.pending()
                                               ? store.poll()
                                               : store.begin_load();
        if (status == SdRepositoryAdapterStatus::Ok && store.ready()) {
            ui_language_set(store.settings().language);
            return true;
        }
        if (status == SdRepositoryAdapterStatus::Pending) {
            vTaskDelay(pdMS_TO_TICKS(40));
            continue;
        }
        const sticky_storage::SdStorageServiceStatus storage =
            sticky_storage::sd_storage_service_status();
        if (storage.card_state == sticky_storage::SdCardState::Mounting ||
            (storage.running && !storage.recovery_known)) {
            vTaskDelay(pdMS_TO_TICKS(40));
            continue;
        }
        break;
    }
    ui_language_set(UiLanguage::English);
    return false;
}

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
        halt_after_error("board_power", power_result);
    }

    // Enable the same active-low battery charging path as the hardware demo.
    // 启用与硬件示例相同的低电平有效电池充电路径。
    const esp_err_t charger_result = board_charger_init();
    if (charger_result != ESP_OK) {
        halt_after_error("board_charger", charger_result);
    }

    // Isolate the other SPI2 device before the e-paper driver owns the bus.
    // 在电子纸驱动接管SPI2前，先让共享总线上的SD卡进入确定的空闲状态。
    const esp_err_t shared_spi_result = board_shared_spi_prepare();
    if (shared_spi_result != ESP_OK) {
        halt_after_error("board_shared_spi", shared_spi_result);
    }

#if defined(STICKY_SD_AUDIO_TEST) && STICKY_SD_AUDIO_TEST
    sticky_sd_audio_test_run();
    return;
#endif

#if defined(STICKY_VOICE_CLASSIFIER_TEST) && STICKY_VOICE_CLASSIFIER_TEST
    sticky_voice_classifier_test_run();
    return;
#endif

    const esp_err_t sensor_bus_result = board_sensor_bus_init();
    if (sensor_bus_result != ESP_OK) {
        halt_after_error("board_sensor_bus", sensor_bus_result);
    }

    // Attach the clock before apps read saved time or offer user calibration.
    // 在APP读取存档时间或提供用户校时前，先挂载RTC设备。
    const esp_err_t rtc_result =
        sticky_rtc_init(board_sensor_i2c_bus());
    if (rtc_result != ESP_OK) {
        STICKY_LOGW(kTag,
                    "component=sticky_rtc result=%s fallback=app_timer",
                    esp_err_to_name(rtc_result));
    }

    // Read percentage from the same BQ27220 fuel gauge used by the hardware
    // reference instead of estimating charge from battery voltage.
    // 使用硬件示例中的BQ27220直接读取百分比，不通过电压估算电量。
    const esp_err_t battery_result =
        sticky_battery_init(board_sensor_i2c_bus());
    if (battery_result != ESP_OK) {
        STICKY_LOGW(kTag,
                    "component=sticky_battery result=%s fallback=unknown_percent",
                    esp_err_to_name(battery_result));
    }

    const esp_err_t display_result = sticky_display_init();
    if (display_result != ESP_OK) {
        halt_after_error("sticky_display_init", display_result);
    }

    Canvas *canvas = sticky_display_canvas();
    if (canvas == nullptr) {
        halt_after_error("sticky_display_canvas", ESP_ERR_INVALID_STATE);
    }

    // A physical white full refresh removes the image retained by e-paper
    // before the first desktop-pet frame becomes the new baseline.
    // 先对白屏执行一次实体全刷，清除电子纸保留的旧画面，再建立桌宠首帧基线。
    const bool woke_from_deep_sleep =
        esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED;
    if (!woke_from_deep_sleep) {
        const esp_err_t clear_result = sticky_display_clear();
        if (clear_result != ESP_OK) {
            halt_after_error("sticky_display_clear", clear_result);
        }
    } else {
        STICKY_LOGI(kTag,
                    "display=clear action=skipped reason=deep_sleep_wake");
    }

#if UI_DEBUG_FONT_TEST
    sticky_display_set_battery_overlay_enabled(false);
#if UI_DEBUG_FONT_TEST_PORTRAIT
    canvas->set_rotation(CanvasRotation::Deg90Clockwise);
#endif
    ui_render_font_test_screen(*canvas);
    const esp_err_t font_test_result = sticky_display_refresh_monochrome();
    if (font_test_result != ESP_OK) {
        halt_after_error("ui_font_test", font_test_result);
    }
    STICKY_LOGI(kTag, "phase=font_test result=ready");
    while (true) {
        vTaskDelay(kHeartbeatInterval);
    }
#endif

    const esp_err_t touch_result = sticky_touch_init();
    if (touch_result != ESP_OK) {
        halt_after_error("sticky_touch_init", touch_result);
    }

#if defined(STICKY_CHINESE_INPUT_TEST) && STICKY_CHINESE_INPUT_TEST
    sticky_chinese_input_test_run(*canvas);
#endif

    // Buzzer and IMU are optional for journaling: run silent or with a fixed
    // orientation instead of halting the whole device.
    // 蜂鸣器和IMU不是记录功能的必要条件：失败时静音或固定方向继续运行。
    const esp_err_t buzzer_result = sticky_buzzer_init();
    if (buzzer_result != ESP_OK) {
        STICKY_LOGW(kTag,
                    "component=sticky_buzzer result=%s fallback=silent",
                    esp_err_to_name(buzzer_result));
    }

    const esp_err_t imu_result =
        sticky_imu_init(board_sensor_i2c_bus());
    if (imu_result != ESP_OK) {
        STICKY_LOGW(kTag,
                    "component=sticky_imu result=%s fallback=fixed_orientation",
                    esp_err_to_name(imu_result));
    }

    // NVS holds only legacy records awaiting migration and the network key.
    // Never erase it automatically here: a full or newer-format partition
    // disables those features while journaling on MicroSD keeps working.
    // NVS仅保存待迁移旧数据，不能自动擦除；初始化失败时跳过这些功能继续启动。
    const esp_err_t nvs_result = nvs_flash_init();
    if (nvs_result != ESP_OK) {
        STICKY_LOGW(kTag,
                    "component=nvs_flash result=%s fallback=legacy_migration_disabled",
                    esp_err_to_name(nvs_result));
    }

    // The storage owner handles card detection and mount lifecycle. Missing or
    // unsupported media must not prevent the current firmware from starting.
    const esp_err_t sd_storage_result =
        sticky_storage::sd_storage_service_start();
    if (sd_storage_result != ESP_OK) {
        STICKY_LOGW(kTag,
                    "component=sd_storage result=%s runtime=disabled",
                    esp_err_to_name(sd_storage_result));
    }

    if (!load_device_preferences_from_sd()) {
        STICKY_LOGW(kTag,
                    "language=load backend=microsd result=unavailable "
                    "fallback=english");
    }

#if defined(STICKY_MANIFEST_COMMIT_TEST) && STICKY_MANIFEST_COMMIT_TEST
    sticky_manifest_commit_test_run(*canvas);
#endif

#if defined(STICKY_JOURNAL_POWER_TEST) && STICKY_JOURNAL_POWER_TEST
    sticky_journal_power_test_run(*canvas);
#endif

#if defined(STICKY_OBJECT_POWER_TEST) && STICKY_OBJECT_POWER_TEST
    sticky_object_power_test_run(*canvas);
#endif

#if defined(STICKY_INDEX_POWER_TEST) && STICKY_INDEX_POWER_TEST
    sticky_index_power_test_run(*canvas);
#endif

#if defined(STICKY_CHECKPOINT_COMMIT_TEST) && STICKY_CHECKPOINT_COMMIT_TEST
    sticky_checkpoint_commit_test_run(*canvas);
#endif

#if defined(STICKY_STORAGE_STATUS_TEST) && STICKY_STORAGE_STATUS_TEST
    sticky_storage_status_test_run(*canvas);
#endif

    // Starts Baby Week as the product home and the framework-owned launcher.
    // 以孕周作为产品主页，并启动框架统一管理的应用选择器。
    const esp_err_t app_result = sticky_app_start(*canvas);
    if (app_result != ESP_OK) {
        halt_after_error("sticky_app_start", app_result);
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
