#include "kick_storage.h"

#include <cstddef>
#include <cstdio>

#include "nvs.h"

namespace {
constexpr char kNamespace[] = "kicks";
constexpr uint32_t kMagic = 0x4B494B31U;
struct Record {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  KickSession item;
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
Record make_record(const KickSession &item) {
  Record record = {kMagic, 1U, sizeof(KickSession), item, 0U};
  record.checksum = checksum(record);
  return record;
}
bool valid(const Record &record) {
  return record.magic == kMagic && record.version == 1U &&
         record.size == sizeof(KickSession) &&
         kick_session_valid(record.item) && record.checksum == checksum(record);
}
void key(size_t slot, char output[8]) {
  std::snprintf(output, 8U, "k%02u", static_cast<unsigned>(slot));
}
} // namespace

esp_err_t kick_storage_load(KickService &service) {
  service.clear();
  nvs_handle_t handle = 0;
  esp_err_t result = nvs_open(kNamespace, NVS_READONLY, &handle);
  if (result == ESP_ERR_NVS_NOT_FOUND)
    return ESP_OK;
  if (result != ESP_OK)
    return result;
  for (size_t slot = 0U; slot < KickService::kCapacity; ++slot) {
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

esp_err_t kick_storage_save(const KickService &service) {
  nvs_handle_t handle = 0;
  esp_err_t result = nvs_open(kNamespace, NVS_READWRITE, &handle);
  if (result != ESP_OK)
    return result;
  for (size_t slot = 0U; slot < KickService::kCapacity && result == ESP_OK;
       ++slot) {
    char name[8] = {};
    key(slot, name);
    const KickSession *item = service.at(slot);
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
