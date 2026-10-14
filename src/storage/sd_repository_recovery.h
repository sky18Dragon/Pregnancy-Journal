#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "sd_index_snapshot.h"
#include "sd_journal_payload.h"

namespace sticky_storage {

constexpr size_t kSdRecoveryRecordCapacity = kSdIndexSnapshotEntryCapacity;

struct SdRecoveredRecord {
    RepositoryKey key = {};
    uint32_t revision = 0U;
    bool tombstone = false;
    uint32_t object_bytes = 0U;
    uint32_t object_crc32 = 0U;
    char object_file[kSdJournalObjectNameBytes + 1U] = {};
};

enum class SdRepositoryRecoveryStatus : uint8_t {
    Ok = 0,
    InvalidArgument,
    UnsupportedPayload,
    NonMonotonicTransaction,
    TransactionConflict,
    RevisionConflict,
    NotFound,
    Tombstoned,
    CapacityReached,
    CheckpointMismatch,
};

struct SdRepositoryRecoveryResult {
    bool changed = false;
    uint64_t transaction_id = 0U;
    size_t mutation_count = 0U;
};

// Bounded in-memory view rebuilt by replaying only complete, CRC-verified
// journal frames. Object files alone are never visible. A whole transaction is
// validated before any slot changes, so recovery cannot expose a partial batch.
class SdRepositoryRecoveryIndex {
  public:
    void reset();
    SdRepositoryRecoveryStatus restore_snapshot(
        const SdIndexSnapshot &snapshot);
    SdRepositoryRecoveryStatus apply(
        SdJournalRecordType type, uint64_t transaction_id,
        const uint8_t *payload, size_t payload_size,
        SdRepositoryRecoveryResult &result);

    SdRepositoryRecoveryStatus read(const RepositoryKey &key,
                                    SdRecoveredRecord &record) const;
    RepositoryStatus list(const RepositoryPageRequest &request,
                          RepositoryPage &page) const;
    bool record_at(size_t slot, SdRecoveredRecord &record) const;

    size_t stored_count() const;
    size_t live_count() const;
    uint64_t last_transaction_id() const;

  private:
    struct Slot {
        bool occupied = false;
        SdRecoveredRecord record = {};
    };

    static bool same_key(const RepositoryKey &left,
                         const RepositoryKey &right);
    int find_slot(const RepositoryKey &key) const;
    int find_free_slot(const std::array<bool, kSdRecoveryRecordCapacity>
                           &reserved) const;

    std::array<Slot, kSdRecoveryRecordCapacity> slots_ = {};
    size_t stored_count_ = 0U;
    size_t live_count_ = 0U;
    uint64_t last_transaction_id_ = 0U;
    uint32_t last_fingerprint_ = 0U;
};

}  // namespace sticky_storage
