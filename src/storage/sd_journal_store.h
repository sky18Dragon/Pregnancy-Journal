#pragma once

#include <cstddef>
#include <cstdint>

#include "sd_journal_file.h"
#include "sd_journal_segments.h"

namespace sticky_storage {

struct SdJournalStorePaths {
    const char *application_root = nullptr;
    const char *journal_directory = nullptr;
};

enum class SdJournalStoreStatus : uint8_t {
    Missing = 0,
    Empty,
    Ready,
    TailTruncated,
    Corrupt,
    UnsupportedSchema,
    UnsupportedRecordType,
    DatasetMismatch,
    NonMonotonicTransaction,
    ConsumerRejected,
    SegmentGap,
    SegmentTooLarge,
    UnexpectedEntry,
    SegmentLimitReached,
    IoError,
    InvalidArgument,
    DirectoryError,
    TransactionConflict,
    OpenError,
    ShortWrite,
    SyncError,
    CloseError,
    VerifyError,
};

struct SdJournalStoreProbe {
    SdJournalStoreStatus status = SdJournalStoreStatus::InvalidArgument;
    uint32_t segment_count = 0U;
    uint32_t active_segment = 0U;
    uint64_t total_bytes = 0U;
    uint32_t total_record_count = 0U;
    SdJournalFileProbe active = {};
};

struct SdJournalStoreAppendResult {
    SdJournalStoreStatus status = SdJournalStoreStatus::InvalidArgument;
    bool changed = false;
    SdJournalStoreProbe probe = {};
};

using SdJournalStoreRecordVisitor = bool (*)(
    void *context, uint32_t segment_number,
    const SdJournalRecordView &record);

// Discovers the contiguous TXN00001.LOG..TXNnnnnn.LOG sequence, rejects gaps
// or unexpected entries, and streams every segment in order. It never creates
// or modifies the directory.
SdJournalStoreProbe sd_journal_store_probe(
    const SdJournalStorePaths &paths, const char *expected_dataset_id);
SdJournalStoreProbe sd_journal_store_replay(
    const SdJournalStorePaths &paths, const char *expected_dataset_id,
    SdJournalStoreRecordVisitor visitor, void *visitor_context);

// Chooses the current or next bounded segment, delegates the durable file
// append, then replays the complete store before confirming success.
SdJournalStoreAppendResult sd_journal_store_append(
    const SdJournalStorePaths &paths, const char *dataset_id,
    SdJournalRecordType type, uint64_t transaction_id,
    const uint8_t *payload, size_t payload_size);

}  // namespace sticky_storage
