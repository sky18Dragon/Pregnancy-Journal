#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "sd_card_state.h"
#include "sd_journal_store.h"
#include "sd_manifest_reader.h"

namespace sticky_storage {

enum class SdJournalRequestOperation : uint8_t {
    Probe = 0,
    Append,
};

enum class SdJournalRequestStatus : uint8_t {
    Ok = 0,
    InvalidArgument,
    CardMissing,
    CardReadOnly,
    NoSpace,
    CardUnavailable,
    CapacityUnavailable,
    StaleGeneration,
    ManifestUnavailable,
    DatasetMismatch,
    DirectoryError,
    ProbeIoError,
    TailTruncated,
    Corrupt,
    UnsupportedSchema,
    UnsupportedRecordType,
    NonMonotonicTransaction,
    TransactionConflict,
    OpenError,
    ShortWrite,
    SyncError,
    CloseError,
    VerifyError,
};

struct SdJournalRequest {
    SdJournalRequestOperation operation = SdJournalRequestOperation::Probe;
    uint32_t mount_generation = 0U;
    char dataset_id[kDatasetIdCharacters + 1U] = {};
    SdJournalRecordType type = SdJournalRecordType::Commit;
    uint64_t transaction_id = 0U;
    std::array<uint8_t, kSdJournalMaxPayloadBytes> payload = {};
    size_t payload_size = 0U;
};

struct SdJournalRequestContext {
    SdCardState card_state = SdCardState::Absent;
    uint32_t mount_generation = 0U;
    SdManifestProbeState manifest_state =
        SdManifestProbeState::NotInitialized;
    bool manifest_selected = false;
    SdManifest manifest = {};
};

struct SdJournalRequestResult {
    SdJournalRequestStatus status = SdJournalRequestStatus::InvalidArgument;
    bool changed = false;
    SdJournalStoreProbe probe = {};
};

bool sd_journal_make_probe_request(uint32_t mount_generation,
                                   const char *dataset_id,
                                   SdJournalRequest &request);
bool sd_journal_make_append_request(
    uint32_t mount_generation, const char *dataset_id,
    SdJournalRecordType type, uint64_t transaction_id,
    const uint8_t *payload, size_t payload_size, SdJournalRequest &request);

SdJournalRequestResult sd_journal_execute_request(
    const SdJournalRequest &request, const SdJournalRequestContext &context,
    const SdJournalStorePaths &paths);
SdJournalRequestResult sd_journal_execute_request_with_ops(
    const SdJournalRequest &request, const SdJournalRequestContext &context,
    const SdJournalFilePaths &paths, const SdJournalFileOps &ops);

}  // namespace sticky_storage
