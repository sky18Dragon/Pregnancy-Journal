#include "board_sd_card.h"
#include "board_shared_spi.h"
#include "pin_config.h"
#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
sdmmc_card_t *card = nullptr;
bool cleanup_pending = false;

esp_err_t restore_idle_and_power_off()
{
    // IDF sdspi deinit_slot() changes CS to INPUT. Preload the high latch,
    // restore output direction, and only then remove card power.
    esp_err_t result = gpio_set_level(static_cast<gpio_num_t>(PIN_SD_CS), 1);
    if (result == ESP_OK) {
        result = gpio_set_direction(
            static_cast<gpio_num_t>(PIN_SD_CS), GPIO_MODE_OUTPUT);
    }
    if (result == ESP_OK) {
        result = gpio_set_level(static_cast<gpio_num_t>(PIN_SD_EN), 0);
    }
    if (result == ESP_OK) {
        cleanup_pending = false;
    }
    return result;
}
}

bool board_sd_card_present()
{
    return gpio_get_level(static_cast<gpio_num_t>(PIN_SD_DETECT)) == 0;
}

esp_err_t board_sd_card_mount()
{
    if (card) return ESP_ERR_INVALID_STATE;
    if (cleanup_pending) {
        const esp_err_t cleanup = restore_idle_and_power_off();
        if (cleanup != ESP_OK) return cleanup;
    }
    if (!board_sd_card_present()) return ESP_ERR_NOT_FOUND;
    esp_err_t result = board_shared_spi_init();
    if (result != ESP_OK) return result;
    gpio_set_level(static_cast<gpio_num_t>(PIN_SD_EN), 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    sdspi_device_config_t device = SDSPI_DEVICE_CONFIG_DEFAULT();
    device.host_id = SPI2_HOST;
    device.gpio_cs = static_cast<gpio_num_t>(PIN_SD_CS);
    esp_vfs_fat_mount_config_t config = {};
    config.format_if_mount_failed = false;
    config.max_files = 4;
    config.allocation_unit_size = 16 * 1024;
    sdmmc_card_t *mounted_card = nullptr;
    result = esp_vfs_fat_sdspi_mount(
        "/sdcard", &host, &device, &config, &mounted_card);
    if (result != ESP_OK) {
        cleanup_pending = true;
        restore_idle_and_power_off();
        return result;
    }
    card = mounted_card;
    return ESP_OK;
}

esp_err_t board_sd_card_unmount()
{
    if (card) {
        const esp_err_t result =
            esp_vfs_fat_sdcard_unmount("/sdcard", card);
        if (result != ESP_OK) return result;
        card = nullptr;
        cleanup_pending = true;
    }
    if (!cleanup_pending) return ESP_ERR_INVALID_STATE;
    // SD卸载会将CS改为输入；清理失败时保留pending状态，允许再次调用完成断电。
    return restore_idle_and_power_off();
}

esp_err_t board_sd_card_space(uint64_t &total_bytes, uint64_t &free_bytes)
{
    total_bytes = 0U;
    free_bytes = 0U;
    if (card == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_vfs_fat_info("/sdcard", &total_bytes, &free_bytes);
}
