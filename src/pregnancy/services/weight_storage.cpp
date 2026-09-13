#include "weight_storage.h"

#include <cstddef>
#include <cstdio>

#include "nvs.h"

namespace {
constexpr char kNamespace[] = "weight";
constexpr uint32_t kMagic = 0x57475431U;
struct Record {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  WeightRecord item;
  uint32_t checksum;
};
uint32_t checksum(const Record &record) {
  const auto *bytes = reinterpret_cast<const uint8_t *>(&record);
  uint32_t hash = 2166136261U;
  for (size_t i = 0U; i < offsetof(Record, checksum); ++i) {
    hash ^= bytes[i];
    hash *= 16777619U;
  }
  return hash;
}
Record make_record(const WeightRecord &item) {
  Record record = {kMagic, 1U, sizeof(WeightRecord), item, 0U};
  record.checksum = checksum(record);
  return record;
}
bool valid(const Record &record) {
  return record.magic == kMagic && record.version == 1U &&
         record.size == sizeof(WeightRecord) &&
         weight_record_valid(record.item) &&
         record.checksum == checksum(record);
}
void key(size_t slot, char output[8]) {
  std::snprintf(output, 8U, "w%02u", static_cast<unsigned>(slot));
}
} // namespace

esp_err_t weight_storage_load(WeightService &service,
                              WeightProfileSettings &settings) {
  service.clear();
  settings = {};
  nvs_handle_t handle = 0;
  esp_err_t result = nvs_open(kNamespace, NVS_READONLY, &handle);
  if (result == ESP_ERR_NVS_NOT_FOUND)
    return ESP_OK;
  if (result != ESP_OK)
    return result;
  size_t profile_size = sizeof(settings);
  if (nvs_get_blob(handle, "profile", &settings, &profile_size) != ESP_OK ||
      profile_size != sizeof(settings) || settings.height_cm < 100U ||
      settings.height_cm > 220U)
    settings = {};
  for (size_t slot = 0U; slot < WeightService::kCapacity; ++slot) {
    char name[8] = {};
    key(slot, name);
    Record record = {};
    size_t size = sizeof(record);
    result = nvs_get_blob(handle, name, &record, &size);
    if (result == ESP_ERR_NVS_NOT_FOUND)
      continue;
    if (result == ESP_OK && size == sizeof(record) && valid(record))
      service.upsert(record.item);
  }
  nvs_close(handle);
  return ESP_OK;
}

esp_err_t weight_storage_save(const WeightService &service,
                              const WeightProfileSettings &settings) {
  nvs_handle_t handle = 0;
  esp_err_t result = nvs_open(kNamespace, NVS_READWRITE, &handle);
  if (result != ESP_OK)
    return result;
  result = nvs_set_blob(handle, "profile", &settings, sizeof(settings));
  for (size_t slot = 0U; result == ESP_OK && slot < WeightService::kCapacity;
       ++slot) {
    char name[8] = {};
    key(slot, name);
    const WeightRecord *item = service.at(slot);
    if (item == nullptr) {
      const esp_err_t erased = nvs_erase_key(handle, name);
      if (erased != ESP_OK && erased != ESP_ERR_NVS_NOT_FOUND)
        result = erased;
    } else {
      const Record record = make_record(*item);
      result = nvs_set_blob(handle, name, &record, sizeof(record));
    }
  }
  if (result == ESP_OK || result == ESP_ERR_NVS_NOT_FOUND)
    result = nvs_commit(handle);
  nvs_close(handle);
  return result;
}
