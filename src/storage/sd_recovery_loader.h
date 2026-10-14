#pragma once

#include <array>
#include <cstdint>

#include "sd_index_snapshot.h"
#include "sd_repository_recovery.h"

namespace sticky_storage {

struct SdRecoveryPaths {
    const char *application_root = nullptr;
    const char *journal_directory = nullptr;
    const char *index_directory = nullptr;
    const char *records_directory = nullptr;
};

enum class SdRecoveryLoadStatus : uint8_t {
    NotAttempted = 0,
    Empty,
    ReadyJournal,
    ReadyCheckpoint,
    TailTruncated,
    JournalInvalid,
    PayloadInvalid,
    CheckpointInvalid,
    IndexMissing,
    IndexMismatch,
    ObjectMissing,
    ObjectMismatch,
    IoError,
    InvalidArgument,
};

struct SdRecoveryLoadResult {
    SdRecoveryLoadStatus status = SdRecoveryLoadStatus::NotAttempted;
    bool view_ready = false;
    bool checkpoint_used = false;
    uint32_t journal_records = 0U;
    uint64_t last_transaction_id = 0U;
    uint64_t checkpoint_transaction_id = 0U;
    uint64_t snapshot_transaction_id = 0U;
    uint32_t stored_records = 0U;
    uint32_t live_records = 0U;
    uint32_t verified_objects = 0U;
    uint32_t index_bytes = 0U;
};

// Kept outside the storage task stack. One workspace is sufficient because
// the storage worker is the sole filesystem owner.
struct SdRecoveryWorkspace {
    std::array<uint8_t, kSdIndexSnapshotMaxBytes> index_bytes = {};
    SdIndexSnapshot snapshot = {};
};

SdRecoveryLoadResult sd_recovery_load(
    const SdRecoveryPaths &paths, const char *dataset_id,
    SdRepositoryRecoveryIndex &index, SdRecoveryWorkspace &workspace);

}  // namespace sticky_storage
