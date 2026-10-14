#include "sd_index_power_fixture.h"

#include <cstdio>
#include <cstring>

#include "sd_journal_power_fixture.h"

namespace sticky_storage {
namespace {

struct FixtureEntryContext {
    uint32_t object_crc32 = 0U;
};

bool fixture_entry(void *context, size_t index,
                   SdIndexSnapshotEntry &entry)
{
    const auto *fixture = static_cast<const FixtureEntryContext *>(context);
    if (fixture == nullptr || index >= kIndexPowerFixtureEntries) {
        return false;
    }
    char record_id[kRepositoryRecordIdCharacters + 1U] = {};
    std::snprintf(record_id, sizeof(record_id), "%032llx",
                  static_cast<unsigned long long>(index + 1U));
    if (!repository_key_make(RepositoryKind::Note, record_id, entry.key)) {
        return false;
    }
    entry.revision = 1U;
    entry.object_bytes = kJournalPowerFixtureObjectBytes;
    entry.object_crc32 = fixture->object_crc32;
    std::strcpy(entry.object_file, kJournalPowerFixtureObjectFile);
    return true;
}

}  // namespace

size_t sd_index_power_fixture_bytes(
    const char *dataset_id, uint64_t last_transaction_id,
    uint8_t *bytes, size_t capacity)
{
    if (bytes == nullptr || capacity < kIndexPowerFixtureBytes) return 0U;
    std::memset(bytes, 0, capacity);
    if (!sd_dataset_id_valid(dataset_id) || last_transaction_id == 0U) {
        return 0U;
    }
    FixtureEntryContext context = {
        sd_journal_power_fixture_object_crc32()};
    return sd_index_snapshot_encode_entries(
        dataset_id, last_transaction_id, kIndexPowerFixtureEntries,
        fixture_entry, &context, bytes, capacity);
}

}  // namespace sticky_storage
