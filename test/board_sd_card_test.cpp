#include <cassert>
#include <string>
#include <vector>
#include "board_sd_card.h"
#include "pin_config.h"
#include "sd_fakes.h"

namespace {
sdmmc_card_t fake_card;
bool present = true, output = true;
int cs = 1, power = 1;
esp_err_t unmount_result = ESP_OK, direction_result = ESP_OK;
esp_err_t mount_result = ESP_OK;
esp_err_t space_result = ESP_OK;
uint64_t total_bytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;
uint64_t free_bytes = 7ULL * 1024ULL * 1024ULL * 1024ULL;
std::vector<int> events;
}
esp_err_t board_shared_spi_init() { return ESP_OK; }
int gpio_get_level(gpio_num_t pin) { assert(pin == PIN_SD_DETECT); return present ? 0 : 1; }
esp_err_t gpio_set_level(gpio_num_t pin, int value)
{
    if (pin == PIN_SD_CS) { cs = value; events.push_back(2); }
    else {
        assert(pin == PIN_SD_EN);
        if (value == 0) assert(output && cs == 1);
        power = value;
        events.push_back(value == 0 ? 4 : 0);
    }
    return ESP_OK;
}
esp_err_t gpio_set_direction(gpio_num_t pin, int mode)
{
    assert(pin == PIN_SD_CS && mode == GPIO_MODE_OUTPUT && cs == 1);
    events.push_back(3);
    if (direction_result == ESP_OK) output = true;
    return direction_result;
}
esp_err_t esp_vfs_fat_sdspi_mount(const char *, const sdmmc_host_t *host,
    const sdspi_device_config_t *device, const esp_vfs_fat_mount_config_t *config, sdmmc_card_t **card)
{
    assert(host->slot == SPI2_HOST && device->host_id == SPI2_HOST && device->gpio_cs == PIN_SD_CS);
    assert(!config->format_if_mount_failed);
    output = true;
    if (mount_result == ESP_OK) *card = &fake_card;
    return mount_result;
}
esp_err_t esp_vfs_fat_sdcard_unmount(const char *, sdmmc_card_t *card)
{
    assert(card == &fake_card);
    events.push_back(1);
    if (unmount_result == ESP_OK) output = false;  // Real IDF deinit behavior.
    return unmount_result;
}
esp_err_t esp_vfs_fat_info(const char *path, uint64_t *total,
                           uint64_t *free)
{
    assert(std::string(path) == "/sdcard");
    if (space_result == ESP_OK) {
        *total = total_bytes;
        *free = free_bytes;
    }
    return space_result;
}
int main()
{
    uint64_t total = 1U, free = 1U;
    assert(board_sd_card_space(total, free) == ESP_ERR_INVALID_STATE);
    assert(total == 0U && free == 0U);
    assert(board_sd_card_unmount() == ESP_ERR_INVALID_STATE);
    present = false;
    assert(board_sd_card_mount() == ESP_ERR_NOT_FOUND);
    present = true;
    mount_result = ESP_FAIL;
    events.clear();
    assert(board_sd_card_mount() == ESP_FAIL);
    assert((events == std::vector<int>{0, 2, 3, 4}));
    assert(power == 0 && output && cs == 1);
    mount_result = ESP_OK;
    assert(board_sd_card_mount() == ESP_OK);
    assert(board_sd_card_space(total, free) == ESP_OK);
    assert(total == total_bytes && free == free_bytes);
    space_result = ESP_FAIL;
    assert(board_sd_card_space(total, free) == ESP_FAIL);
    assert(total == 0U && free == 0U);
    space_result = ESP_OK;
    assert(board_sd_card_mount() == ESP_ERR_INVALID_STATE);
    events.clear();
    assert(board_sd_card_unmount() == ESP_OK);
    assert((events == std::vector<int>{1, 2, 3, 4}));
    assert(power == 0 && output && cs == 1);
    assert(board_sd_card_mount() == ESP_OK);
    unmount_result = ESP_FAIL;
    events.clear();
    assert(board_sd_card_unmount() == ESP_FAIL);
    assert((events == std::vector<int>{1}) && power == 1);
    unmount_result = ESP_OK;
    direction_result = ESP_FAIL;
    events.clear();
    assert(board_sd_card_unmount() == ESP_FAIL);
    assert((events == std::vector<int>{1, 2, 3}) && power == 1);
    direction_result = ESP_OK;
    events.clear();
    assert(board_sd_card_unmount() == ESP_OK);
    assert((events == std::vector<int>{2, 3, 4}));
    assert(power == 0 && output && cs == 1);
    assert(board_sd_card_unmount() == ESP_ERR_INVALID_STATE);
}
