#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "sd_manifest.h"
#include "storage_contract.h"

namespace sticky_storage {

constexpr size_t kRepositoryRecordIdCharacters = 32U;
constexpr size_t kRepositoryPayloadBytes = 1024U;
constexpr size_t kRepositoryBatchMutations = 4U;
constexpr size_t kRepositoryPageEntries = 8U;

enum class RepositoryKind : uint8_t {
    Pregnancy = 1,
    Reminder = 2,
    Checkup = 3,
    Weight = 4,
    Habit = 5,
    HabitEvent = 6,
    Note = 7,
    VoiceTask = 8,
    Preference = 9,
    Kicks = 10,  // Appended; existing persisted kind values stay unchanged.
};

struct RepositoryKey {
    RepositoryKind repository = RepositoryKind::Pregnancy;
    char record_id[kRepositoryRecordIdCharacters + 1U] = {};
};

struct RepositoryRecord {
    RepositoryKey key = {};
    uint32_t revision = 0U;
    bool tombstone = false;
    std::array<uint8_t, kRepositoryPayloadBytes> payload = {};
    size_t payload_size = 0U;
};

enum class RepositoryMutationType : uint8_t {
    Upsert = 0,
    Delete,
};

struct RepositoryMutation {
    RepositoryMutationType type = RepositoryMutationType::Upsert;
    RepositoryKey key = {};
    uint32_t expected_revision = 0U;
    std::array<uint8_t, kRepositoryPayloadBytes> payload = {};
    size_t payload_size = 0U;
};

struct RepositoryBatch {
    uint64_t transaction_id = 0U;
    std::array<RepositoryMutation, kRepositoryBatchMutations> mutations = {};
    size_t mutation_count = 0U;
};

struct RepositoryBatchResult {
    bool changed = false;
    uint64_t transaction_id = 0U;
    size_t mutation_count = 0U;
    std::array<uint32_t, kRepositoryBatchMutations> revisions = {};
};

struct RepositoryPageRequest {
    RepositoryKind repository = RepositoryKind::Pregnancy;
    uint32_t cursor = 0U;
    size_t limit = kRepositoryPageEntries;
    // Domain loaders use this to preserve numeric-ID high watermarks across
    // deletion. Ordinary user-facing lists leave it false.
    bool include_tombstones = false;
};

struct RepositoryPageEntry {
    char record_id[kRepositoryRecordIdCharacters + 1U] = {};
    uint32_t revision = 0U;
    bool tombstone = false;
};

struct RepositoryPage {
    std::array<RepositoryPageEntry, kRepositoryPageEntries> entries = {};
    size_t count = 0U;
    uint32_t next_cursor = 0U;
    bool has_more = false;
};

enum class RepositoryStatus : uint8_t {
    Ok = 0,
    InvalidArgument,
    NotFound,
    Tombstoned,
    RevisionConflict,
    TransactionConflict,
    CapacityReached,
    Unsupported,
    IoError,
};

bool repository_kind_valid(RepositoryKind kind);
bool repository_key_valid(const RepositoryKey &key);
bool repository_key_make(RepositoryKind repository, const char *record_id,
                         RepositoryKey &key);
bool repository_mutation_make_upsert(
    const RepositoryKey &key, uint32_t expected_revision,
    const uint8_t *payload, size_t payload_size,
    RepositoryMutation &mutation);
bool repository_mutation_make_delete(const RepositoryKey &key,
                                     uint32_t expected_revision,
                                     RepositoryMutation &mutation);

class StorageRepository {
  public:
    virtual ~StorageRepository() = default;

    virtual RepositoryStatus read(const RepositoryKey &key,
                                  RepositoryRecord &record) const = 0;
    virtual RepositoryStatus list(const RepositoryPageRequest &request,
                                  RepositoryPage &page) const = 0;
    virtual RepositoryStatus commit(const RepositoryMutation &mutation,
                                    uint64_t transaction_id,
                                    RepositoryBatchResult &result) = 0;
    virtual RepositoryStatus erase(const RepositoryKey &key,
                                   uint32_t expected_revision,
                                   uint64_t transaction_id,
                                   RepositoryBatchResult &result) = 0;
    virtual RepositoryStatus commit_batch(
        const RepositoryBatch &batch, RepositoryBatchResult &result) = 0;
};

}  // namespace sticky_storage
