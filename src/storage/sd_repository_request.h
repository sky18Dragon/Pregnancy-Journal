#pragma once

#include <cstddef>
#include <cstdint>

#include "sd_card_state.h"
#include "sd_journal_store.h"
#include "sd_manifest_reader.h"
#include "sd_record_object_file.h"
#include "sd_repository_recovery.h"
#include "storage_repository.h"

namespace sticky_storage {

// Service-level sentinel. The worker replaces it with the next transaction ID
// immediately before executing a queued mutation; zero is never persisted.
constexpr uint64_t kRepositoryAutoTransactionId = 0U;

enum class SdRepositoryRequestOperation : uint8_t {
    Read = 0,
    List,
    Commit,
    Erase,
};

enum class SdRepositoryRequestStatus : uint8_t {
    Ok = 0,
    InvalidArgument,
    CardMissing,
    CardReadOnly,
    CardFull,
    CardUnavailable,
    CapacityUnavailable,
    StaleGeneration,
    ManifestUnavailable,
    DatasetMismatch,
    RecoveryUnavailable,
    NotFound,
    Tombstoned,
    RevisionConflict,
    TransactionConflict,
    CapacityReached,
    ObjectError,
    JournalError,
    RecoveryError,
};

struct SdRepositoryRequest {
    SdRepositoryRequestOperation operation =
        SdRepositoryRequestOperation::Read;
    uint32_t mount_generation = 0U;
    char dataset_id[kDatasetIdCharacters + 1U] = {};
    RepositoryKey key = {};
    RepositoryPageRequest page = {};
    RepositoryMutation mutation = {};
    uint64_t transaction_id = 0U;
};

struct SdRepositoryRequestContext {
    SdCardState card_state = SdCardState::Absent;
    uint32_t mount_generation = 0U;
    bool capacity_known = false;
    uint64_t free_bytes = 0U;
    SdManifestProbeState manifest_state =
        SdManifestProbeState::NotInitialized;
    bool manifest_selected = false;
    SdManifest manifest = {};
    bool recovery_view_ready = false;
};

struct SdRepositoryRequestPaths {
    const char *application_root = nullptr;
    const char *records_directory = nullptr;
    SdJournalStorePaths journal = {};
};

struct SdRepositoryRequestResult {
    SdRepositoryRequestStatus status =
        SdRepositoryRequestStatus::InvalidArgument;
    bool changed = false;
    RepositoryRecord record = {};
    RepositoryPage page = {};
    RepositoryBatchResult batch = {};
    SdRecordObjectWriteStatus object_status =
        SdRecordObjectWriteStatus::InvalidArgument;
    SdRecordObjectReadStatus object_read_status =
        SdRecordObjectReadStatus::InvalidArgument;
    SdJournalStoreStatus journal_status =
        SdJournalStoreStatus::InvalidArgument;
    SdRepositoryRecoveryStatus recovery_status =
        SdRepositoryRecoveryStatus::InvalidArgument;
};

bool sd_repository_make_read_request(
    uint32_t mount_generation, const char *dataset_id,
    const RepositoryKey &key, SdRepositoryRequest &request);
bool sd_repository_make_list_request(
    uint32_t mount_generation, const char *dataset_id,
    const RepositoryPageRequest &page, SdRepositoryRequest &request);
bool sd_repository_make_commit_request(
    uint32_t mount_generation, const char *dataset_id,
    const RepositoryMutation &mutation, uint64_t transaction_id,
    SdRepositoryRequest &request);
bool sd_repository_make_erase_request(
    uint32_t mount_generation, const char *dataset_id,
    const RepositoryKey &key, uint32_t expected_revision,
    uint64_t transaction_id, SdRepositoryRequest &request);

// Generates the immutable content-addressed 8.3 filename used by an upsert.
bool sd_repository_object_name(const uint8_t *payload, size_t payload_size,
                               char name[kSdJournalObjectNameBytes + 1U]);

// Executes on the single storage worker. A successful mutation means the
// object was verified, the journal frame was durable, and the in-memory view
// accepted the same transaction. The service performs a full recovery refresh
// before publishing completion to callers.
SdRepositoryRequestResult sd_repository_execute_request(
    const SdRepositoryRequest &request,
    const SdRepositoryRequestContext &context,
    const SdRepositoryRequestPaths &paths,
    SdRepositoryRecoveryIndex &index);

// Compare disk recovery with the transaction actually assigned to the commit,
// rather than the zero sentinel from an automatic-transaction request.
void sd_repository_validate_recovered_commit(
    SdRepositoryRequestResult &result, bool view_ready,
    uint64_t recovered_transaction_id);

}  // namespace sticky_storage
