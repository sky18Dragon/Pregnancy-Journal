#pragma once
#include <cstdint>
#include "esp_err.h"
using gpio_num_t = int;
constexpr int GPIO_MODE_OUTPUT = 2;
constexpr int SPI2_HOST = 1;
int gpio_get_level(gpio_num_t);
esp_err_t gpio_set_level(gpio_num_t, int);
esp_err_t gpio_set_direction(gpio_num_t, int);
struct sdmmc_card_t {};
struct sdmmc_host_t { int slot; };
struct sdspi_device_config_t { int host_id; gpio_num_t gpio_cs; };
struct esp_vfs_fat_mount_config_t {
    bool format_if_mount_failed;
    int max_files;
    int allocation_unit_size;
};
#define SDSPI_HOST_DEFAULT() sdmmc_host_t{}
#define SDSPI_DEVICE_CONFIG_DEFAULT() sdspi_device_config_t{}
#define pdMS_TO_TICKS(value) (value)
inline void vTaskDelay(int) {}
esp_err_t esp_vfs_fat_sdspi_mount(const char *, const sdmmc_host_t *,
    const sdspi_device_config_t *, const esp_vfs_fat_mount_config_t *, sdmmc_card_t **);
esp_err_t esp_vfs_fat_sdcard_unmount(const char *, sdmmc_card_t *);
esp_err_t esp_vfs_fat_info(const char *, uint64_t *, uint64_t *);
