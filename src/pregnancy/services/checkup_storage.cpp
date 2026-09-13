#include "checkup_storage.h"

#include <cstddef>
#include <cstdio>

#include "app_log.h"
#include "nvs.h"

namespace {
constexpr char kTag[] = "CHECKUP";
constexpr char kNamespace[] = "checkups";
constexpr uint32_t kMagic = 0x43484B31U;
struct Record {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    Checkup item;
    uint32_t checksum;
};
uint32_t checksum(const Record &record)
{
    const auto *bytes = reinterpret_cast<const uint8_t *>(&record);
    uint32_t hash = 2166136261U;
    for (size_t i = 0U; i < offsetof(Record, checksum); ++i) {
        hash ^= bytes[i]; hash *= 16777619U;
    }
    return hash;
}
Record make_record(const Checkup &item)
{
    Record result = {kMagic, 1U, sizeof(Checkup), item, 0U};
    result.checksum = checksum(result);
    return result;
}
bool valid(const Record &record)
{
    return record.magic == kMagic && record.version == 1U &&
           record.size == sizeof(Checkup) && checkup_valid(record.item) &&
           record.checksum == checksum(record);
}
void key(size_t slot, char output[8])
{ std::snprintf(output, 8U, "c%02u", static_cast<unsigned>(slot)); }
}  // namespace

esp_err_t checkup_storage_load(CheckupService &service)
{
    service.clear();
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (result != ESP_OK) return result;
    for (size_t slot = 0U; slot < CheckupService::kCapacity; ++slot) {
        char name[8] = {}; key(slot, name);
        Record record = {}; size_t size = sizeof(record);
        result = nvs_get_blob(handle, name, &record, &size);
        if (result == ESP_ERR_NVS_NOT_FOUND) continue;
        if (result == ESP_OK && size == sizeof(record) && valid(record))
            service.create(record.item);
        else STICKY_LOGW(kTag, "storage=load slot=%u result=skipped_corrupt",
                         static_cast<unsigned>(slot));
    }
    nvs_close(handle);
    return ESP_OK;
}

esp_err_t checkup_storage_save(const CheckupService &service)
{
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (result != ESP_OK) return result;
    for (size_t slot = 0U; slot < CheckupService::kCapacity; ++slot) {
        char name[8] = {}; key(slot, name);
        const Checkup *item = service.at(slot);
        if (item == nullptr) {
            const esp_err_t erased = nvs_erase_key(handle, name);
            if (erased != ESP_OK && erased != ESP_ERR_NVS_NOT_FOUND) {
                result = erased; break;
            }
        } else {
            const Record record = make_record(*item);
            result = nvs_set_blob(handle, name, &record, sizeof(record));
            if (result != ESP_OK) break;
        }
    }
    if (result == ESP_OK || result == ESP_ERR_NVS_NOT_FOUND) result = nvs_commit(handle);
    nvs_close(handle);
    return result;
}

bool checkup_storage_next_event(uint32_t now, uint32_t &epoch)
{
    CheckupService service;
    return checkup_storage_load(service) == ESP_OK && service.next(now, &epoch) != nullptr;
}
