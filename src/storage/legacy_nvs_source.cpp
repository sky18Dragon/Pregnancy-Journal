#include "legacy_nvs_source.h"

#include <cstddef>
#include <cstdio>

#include "kick_service.h"
#include "kick_storage_record.h"
#include "nvs.h"
#include "persistent_state.h"
#include "pregnancy_storage_record.h"
#include "weight_service.h"
#include "weight_storage.h"

namespace sticky_storage {
namespace {

// The retired NVS kick store had a fixed 42-slot layout.
constexpr size_t kLegacyKickSlots = 42U;

constexpr uint32_t kPregnancySlot = 0U;
constexpr uint32_t kWeightFirst = 1U;
constexpr uint32_t kWeightPreferencesSlot =
    kWeightFirst + 32U;  // Legacy firmware had exactly 32 weight slots.
constexpr uint32_t kDevicePreferencesSlot = kWeightPreferencesSlot + 1U;
static_assert(kDevicePreferencesSlot + 1U == LegacyNvsSource::kSlotCount);

struct PregnancyStorageRecordV1 {
    uint32_t magic;
    uint16_t version;
    uint16_t due_year;
    uint8_t due_month;
    uint8_t due_day;
    uint8_t reserved[2];
    uint32_t checksum;
};

uint32_t checksum_v1(const PregnancyStorageRecordV1 &record)
{
    constexpr uint32_t kOffset = 2166136261U;
    constexpr uint32_t kPrime = 16777619U;
    const auto *bytes = reinterpret_cast<const uint8_t *>(&record);
    uint32_t checksum = kOffset;
    for (size_t index = 0U;
         index < offsetof(PregnancyStorageRecordV1, checksum); ++index) {
        checksum ^= bytes[index];
        checksum *= kPrime;
    }
    return checksum;
}

LegacyMigrationReadStatus open_readonly(const char *name,
                                        nvs_handle_t &handle)
{
    const esp_err_t result = nvs_open(name, NVS_READONLY, &handle);
    if (result == ESP_OK) return LegacyMigrationReadStatus::Record;
    return result == ESP_ERR_NVS_NOT_FOUND
               ? LegacyMigrationReadStatus::Missing
               : LegacyMigrationReadStatus::IoError;
}

template <typename Value>
LegacyMigrationReadStatus read_exact(nvs_handle_t handle, const char *key,
                                     Value &value)
{
    size_t size = sizeof(value);
    const esp_err_t result = nvs_get_blob(handle, key, &value, &size);
    if (result == ESP_ERR_NVS_NOT_FOUND)
        return LegacyMigrationReadStatus::Missing;
    if (result != ESP_OK) {
        return result == ESP_ERR_NVS_INVALID_LENGTH
                   ? LegacyMigrationReadStatus::Corrupt
                   : LegacyMigrationReadStatus::IoError;
    }
    return size == sizeof(value) ? LegacyMigrationReadStatus::Record
                                 : LegacyMigrationReadStatus::Corrupt;
}

bool encode_ok(pregnancy_repository::RecordCodecStatus status)
{
    return status == pregnancy_repository::RecordCodecStatus::Ok;
}

LegacyMigrationReadStatus read_pregnancy(LegacyMigrationRecord &output)
{
    nvs_handle_t handle = 0;
    const LegacyMigrationReadStatus opened = open_readonly("pregnancy", handle);
    if (opened != LegacyMigrationReadStatus::Record) return opened;

    constexpr const char *kKeys[] = {"config", "config_b"};
    bool corrupt_seen = false;
    for (const char *key : kKeys) {
        uint8_t bytes[sizeof(PregnancyStorageRecord)] = {};
        size_t size = sizeof(bytes);
        const esp_err_t result = nvs_get_blob(handle, key, bytes, &size);
        if (result == ESP_ERR_NVS_NOT_FOUND) continue;
        if (result != ESP_OK) {
            if (result == ESP_ERR_NVS_INVALID_LENGTH) corrupt_seen = true;
            else {
                nvs_close(handle);
                return LegacyMigrationReadStatus::IoError;
            }
            continue;
        }

        PregnancyProfile profile = {};
        bool valid = false;
        if (size == sizeof(PregnancyStorageRecord)) {
            const auto &stored =
                *reinterpret_cast<const PregnancyStorageRecord *>(bytes);
            if (pregnancy_storage_record_validate(stored)) {
                profile = pregnancy_storage_record_profile(stored);
                valid = true;
            }
        } else if (size == sizeof(PregnancyStorageRecordV1)) {
            const auto &stored =
                *reinterpret_cast<const PregnancyStorageRecordV1 *>(bytes);
            valid = stored.magic == kPregnancyStorageMagic &&
                    stored.version == 1U &&
                    stored.checksum == checksum_v1(stored) &&
                    pregnancy_profile_from_due_date(
                        {stored.due_year, stored.due_month, stored.due_day},
                        profile);
        }
        if (!valid) {
            corrupt_seen = true;
            continue;
        }
        const bool encoded =
            pregnancy_repository::pregnancy_profile_key(output.key) &&
            encode_ok(pregnancy_repository::encode_pregnancy_profile(
                profile, output.payload));
        nvs_close(handle);
        return encoded ? LegacyMigrationReadStatus::Record
                       : LegacyMigrationReadStatus::Corrupt;
    }
    nvs_close(handle);
    return corrupt_seen ? LegacyMigrationReadStatus::Corrupt
                        : LegacyMigrationReadStatus::Missing;
}

LegacyMigrationReadStatus read_weight(uint32_t index,
                                      LegacyMigrationRecord &output)
{
    nvs_handle_t handle = 0;
    const LegacyMigrationReadStatus opened = open_readonly("weight", handle);
    if (opened != LegacyMigrationReadStatus::Record) return opened;
    char key[8] = {};
    std::snprintf(key, sizeof(key), "w%02u", static_cast<unsigned>(index));
    WeightStorageRecord stored = {};
    const LegacyMigrationReadStatus read = read_exact(handle, key, stored);
    nvs_close(handle);
    if (read != LegacyMigrationReadStatus::Record) return read;
    if (!weight_storage_record_valid(stored) ||
        !pregnancy_repository::weight_key(stored.item.id, output.key) ||
        !encode_ok(pregnancy_repository::encode_weight(stored.item,
                                                       output.payload))) {
        return LegacyMigrationReadStatus::Corrupt;
    }
    return LegacyMigrationReadStatus::Record;
}

LegacyMigrationReadStatus read_weight_preferences(
    LegacyMigrationRecord &output)
{
    nvs_handle_t handle = 0;
    const LegacyMigrationReadStatus opened = open_readonly("weight", handle);
    if (opened != LegacyMigrationReadStatus::Record) return opened;
    WeightProfileSettings stored = {};
    const LegacyMigrationReadStatus read = read_exact(handle, "profile", stored);
    nvs_close(handle);
    if (read != LegacyMigrationReadStatus::Record) return read;
    if (!pregnancy_repository::weight_preferences_key(output.key) ||
        !encode_ok(pregnancy_repository::encode_weight_preferences(
            stored, output.payload))) {
        return LegacyMigrationReadStatus::Corrupt;
    }
    return LegacyMigrationReadStatus::Record;
}

bool read_persistent_slot(nvs_handle_t handle, const char *key,
                          StickyPersistentRecord &record,
                          bool &present)
{
    const LegacyMigrationReadStatus read = read_exact(handle, key, record);
    present = read != LegacyMigrationReadStatus::Missing;
    return read == LegacyMigrationReadStatus::Record &&
           sticky_persistent_valid(record);
}

LegacyMigrationReadStatus read_device_preferences(
    LegacyMigrationRecord &output)
{
    nvs_handle_t handle = 0;
    const LegacyMigrationReadStatus opened = open_readonly("sticky_core", handle);
    if (opened != LegacyMigrationReadStatus::Record) return opened;
    StickyPersistentRecord first = {};
    StickyPersistentRecord second = {};
    bool first_present = false;
    bool second_present = false;
    const bool first_valid =
        read_persistent_slot(handle, "settings_a", first, first_present);
    const bool second_valid =
        read_persistent_slot(handle, "settings_b", second, second_present);
    nvs_close(handle);
    StickyPersistentRecord selected = {};
    if (!sticky_persistent_select(first_valid ? &first : nullptr,
                                  second_valid ? &second : nullptr, selected)) {
        return first_present || second_present
                   ? LegacyMigrationReadStatus::Corrupt
                   : LegacyMigrationReadStatus::Missing;
    }
    if (!pregnancy_repository::device_preferences_key(output.key) ||
        !encode_ok(pregnancy_repository::encode_device_preferences(
            sticky_persistent_settings(selected), output.payload))) {
        return LegacyMigrationReadStatus::Corrupt;
    }
    return LegacyMigrationReadStatus::Record;
}

bool erase_namespace(const char *name)
{
    nvs_handle_t handle = 0;
    const esp_err_t opened = nvs_open(name, NVS_READWRITE, &handle);
    if (opened == ESP_ERR_NVS_NOT_FOUND) return true;
    if (opened != ESP_OK) return false;
    esp_err_t result = nvs_erase_all(handle);
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    return result == ESP_OK;
}

}  // namespace

LegacyMigrationReadStatus LegacyNvsSource::read(
    uint32_t slot, LegacyMigrationRecord &record)
{
    record = {};
    if (slot == kPregnancySlot) return read_pregnancy(record);
    if (slot >= kWeightFirst && slot < kWeightPreferencesSlot)
        return read_weight(slot - kWeightFirst, record);
    if (slot == kWeightPreferencesSlot)
        return read_weight_preferences(record);
    if (slot == kDevicePreferencesSlot)
        return read_device_preferences(record);
    return LegacyMigrationReadStatus::Corrupt;
}

bool LegacyNvsSource::legacy_kicks_present() const
{
    nvs_handle_t handle = 0;
    if (nvs_open("kicks", NVS_READONLY, &handle) != ESP_OK) return false;
    for (size_t slot = 0U; slot < kLegacyKickSlots; ++slot) {
        char key[8] = {};
        std::snprintf(key, sizeof(key), "k%02u",
                      static_cast<unsigned>(slot));
        size_t size = 0U;
        if (nvs_get_blob(handle, key, nullptr, &size) == ESP_OK) {
            nvs_close(handle);
            return true;
        }
    }
    nvs_close(handle);
    return false;
}

bool LegacyNvsSource::clear_migrated_namespaces()
{
    // Kicks has its own independently verified import and cleanup. Other
    // namespaces, including those of the retired reminder and checkup apps,
    // are never touched.
    constexpr const char *kNamespaces[] = {
        "pregnancy", "weight", "sticky_core"};
    for (const char *name : kNamespaces) {
        if (!erase_namespace(name)) return false;
    }
    return true;
}

LegacyMigrationReadStatus LegacyKickNvsSource::read(
    uint32_t slot, LegacyMigrationRecord &output)
{
    output = {};
    if (slot >= kLegacyKickSlots) return LegacyMigrationReadStatus::Corrupt;
    nvs_handle_t handle = 0;
    const auto opened = open_readonly("kicks", handle);
    if (opened != LegacyMigrationReadStatus::Record) return opened;
    char key[8] = {};
    std::snprintf(key, sizeof(key), "k%02u", static_cast<unsigned>(slot));
    KickStorageRecord stored = {};
    const auto read = read_exact(handle, key, stored);
    nvs_close(handle);
    if (read != LegacyMigrationReadStatus::Record) return read;
    if (!kick_storage_record_valid(stored) ||
        !pregnancy_repository::kick_key(stored.item.id, output.key) ||
        !encode_ok(pregnancy_repository::encode_kick(
            kick_session_from_v1(stored.item), output.payload))) {
        return LegacyMigrationReadStatus::Corrupt;
    }
    return LegacyMigrationReadStatus::Record;
}

bool LegacyKickNvsSource::clear_migrated_namespaces()
{
    return erase_namespace("kicks");
}

LegacyKickNvsSource &legacy_kick_nvs_source()
{
    static LegacyKickNvsSource source;
    return source;
}

LegacyNvsSource &legacy_nvs_source()
{
    static LegacyNvsSource source;
    return source;
}

}  // namespace sticky_storage
