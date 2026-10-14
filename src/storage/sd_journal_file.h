#pragma once

#include <cstddef>
#include <cstdint>

#include "sd_journal.h"

namespace sticky_storage {

struct SdJournalFilePaths {
    const char *application_root = nullptr;
    const char *journal_directory = nullptr;
    const char *segment = nullptr;
};

struct SdJournalRecordSummary {
    SdJournalRecordType type = SdJournalRecordType::Commit;
    uint64_t transaction_id = 0U;
    size_t encoded_size = 0U;
    size_t payload_size = 0U;
    uint32_t payload_crc32 = 0U;
    uint32_t frame_crc32 = 0U;
};

enum class SdJournalFileProbeStatus : uint8_t {
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
    IoError,
    InvalidArgument,
};

struct SdJournalFileProbe {
    SdJournalFileProbeStatus status =
        SdJournalFileProbeStatus::InvalidArgument;
    uint64_t file_bytes = 0U;
    uint64_t valid_bytes = 0U;
    uint32_t record_count = 0U;
    uint64_t first_transaction_id = 0U;
    SdJournalRecordSummary last = {};
};

enum class SdJournalAppendIoResult : uint8_t {
    Ok = 0,
    OpenError,
    ShortWrite,
    SyncError,
    CloseError,
};

enum class SdJournalAppendStatus : uint8_t {
    Ok = 0,
    InvalidArgument,
    DirectoryError,
    ProbeIoError,
    TailTruncated,
    Corrupt,
    UnsupportedSchema,
    UnsupportedRecordType,
    DatasetMismatch,
    NonMonotonicTransaction,
    TransactionConflict,
    OpenError,
    ShortWrite,
    SyncError,
    CloseError,
    VerifyError,
};

struct SdJournalAppendResult {
    SdJournalAppendStatus status = SdJournalAppendStatus::InvalidArgument;
    bool changed = false;
    SdJournalFileProbe probe = {};
};

struct SdJournalFileOps {
    void *context = nullptr;
    bool (*ensure_directory)(void *context, const char *path) = nullptr;
    SdJournalAppendIoResult (*append_record)(void *context, const char *path,
                                             const uint8_t *bytes,
                                             size_t size) = nullptr;
    SdJournalFileProbe (*probe_segment)(void *context, const char *path,
                                        const char *expected_dataset_id) =
        nullptr;
};

// Streams and validates a segment using one fixed maximum-frame buffer. It
// never creates, truncates, repairs or deletes a path.
SdJournalFileProbe sd_journal_probe_file(const char *path,
                                         const char *expected_dataset_id);
SdJournalFileProbe sd_journal_replay_file(
    const char *path, const char *expected_dataset_id,
    SdJournalRecordVisitor visitor, void *visitor_context);

// Appends one canonical frame, flushes and fsyncs it, closes the file, then
// streams the segment again. Success is returned only when the requested frame
// is the last verified transaction. A retry of that exact last frame is
// idempotent; a damaged or truncated tail always blocks further appends.
SdJournalAppendResult sd_journal_append_file(
    const SdJournalFilePaths &paths, const char *dataset_id,
    SdJournalRecordType type, uint64_t transaction_id,
    const uint8_t *payload, size_t payload_size);
SdJournalAppendResult sd_journal_append_file_with_ops(
    const SdJournalFilePaths &paths, const char *dataset_id,
    SdJournalRecordType type, uint64_t transaction_id,
    const uint8_t *payload, size_t payload_size,
    const SdJournalFileOps &ops);

}  // namespace sticky_storage
