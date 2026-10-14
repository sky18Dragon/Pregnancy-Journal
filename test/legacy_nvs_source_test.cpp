#include <cassert>
#include <cstring>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "kick_storage_record.h"
#include "legacy_nvs_source.h"
#include "nvs.h"

namespace {
std::map<std::string, std::map<std::string, std::vector<uint8_t>>> namespaces;
std::map<nvs_handle_t, std::string> handles;
unsigned writes = 0U;

KickStorageRecord legacy_record(uint32_t id) {
  KickStorageRecord record = {};
  record.magic = 0x4B494B31U;
  record.version = 1U;
  record.size = sizeof(KickSessionV1);
  record.item.schema_version = kKickSessionLegacySchemaVersion;
  record.item.id = id;
  record.item.date = {2026U, 10U, 10U};
  record.item.hour = 9U;
  record.item.duration_minutes = 60U;
  record.item.count = 12U;
  std::strcpy(record.item.note, "old session");
  const auto *bytes = reinterpret_cast<const uint8_t *>(&record);
  record.checksum = 2166136261U;
  for (size_t i = 0U; i < offsetof(KickStorageRecord, checksum); ++i) {
    record.checksum ^= bytes[i];
    record.checksum *= 16777619U;
  }
  return record;
}

void put(const char *key, const KickStorageRecord &record) {
  const auto *bytes = reinterpret_cast<const uint8_t *>(&record);
  namespaces["kicks"][key] = {bytes, bytes + sizeof(record)};
}
}  // namespace

esp_err_t nvs_open(const char *name, nvs_open_mode_t mode, nvs_handle_t *handle) {
  if (namespaces.count(name) == 0U) return ESP_ERR_NVS_NOT_FOUND;
  if (mode == NVS_READWRITE) ++writes;
  *handle = static_cast<nvs_handle_t>(handles.size() + 1U);
  handles[*handle] = name;
  return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *output, size_t *size) {
  const auto &data = namespaces.at(handles.at(handle));
  const auto found = data.find(key);
  if (found == data.end()) return ESP_ERR_NVS_NOT_FOUND;
  if (output == nullptr) { *size = found->second.size(); return ESP_OK; }
  if (*size < found->second.size()) return ESP_ERR_NVS_INVALID_LENGTH;
  *size = found->second.size();
  std::memcpy(output, found->second.data(), *size);
  return ESP_OK;
}
esp_err_t nvs_erase_all(nvs_handle_t handle) {
  ++writes;
  namespaces.at(handles.at(handle)).clear();
  return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t) { ++writes; return ESP_OK; }
void nvs_close(nvs_handle_t handle) { handles.erase(handle); }

int main() {
  using namespace sticky_storage;
  LegacyKickNvsSource kicks;
  LegacyNvsSource common;
  LegacyMigrationRecord migrated = {};
  assert(kicks.read(0U, migrated) == LegacyMigrationReadStatus::Missing);
  for (unsigned slot = 0U; slot < 42U; ++slot) {
    char key[8] = {};
    std::snprintf(key, sizeof(key), "k%02u", slot);
    put(key, legacy_record(slot + 1U));
  }
  const auto before = namespaces;
  assert(kicks.slot_count() == 42U && common.slot_count() == 55U);
  for (unsigned slot = 0U; slot < 42U; ++slot) {
    assert(kicks.read(slot, migrated) == LegacyMigrationReadStatus::Record);
    assert(migrated.key.repository == RepositoryKind::Kicks);
    KickSession decoded = {};
    assert(pregnancy_repository::decode_kick(migrated.payload.bytes.data(),
                                            migrated.payload.size, decoded) ==
           pregnancy_repository::RecordCodecStatus::Ok);
    assert(decoded.id == slot + 1U && decoded.count == 12U);
  }
  assert(writes == 0U && namespaces == before);
  assert(kicks.read(42U, migrated) == LegacyMigrationReadStatus::Corrupt);
  auto broken = legacy_record(1U);
  broken.checksum ^= 1U;
  put("k00", broken);
  assert(kicks.read(0U, migrated) == LegacyMigrationReadStatus::Corrupt);
  namespaces["kicks"]["k00"].resize(1U);
  assert(kicks.read(0U, migrated) == LegacyMigrationReadStatus::Corrupt);
  namespaces["kicks"]["k00"].resize(100U);
  assert(kicks.read(0U, migrated) == LegacyMigrationReadStatus::Corrupt);

  for (const char *name : {"pregnancy", "weight", "sticky_core", "reminders", "checkups", "wifi"})
    namespaces[name]["sentinel"] = {1U};
  assert(common.clear_migrated_namespaces());
  assert(!namespaces["kicks"].empty());
  assert(!namespaces["reminders"].empty() && !namespaces["checkups"].empty());
  assert(namespaces["pregnancy"].empty() && namespaces["weight"].empty());
  assert(kicks.clear_migrated_namespaces());
  assert(namespaces["kicks"].empty());
  assert(!namespaces["reminders"].empty() && !namespaces["checkups"].empty());
  assert(!namespaces["wifi"].empty());
}
