#pragma once

#include <cstddef>
#include <cstdint>

#include "sd_manifest.h"

namespace sticky_storage {

constexpr uint16_t kSdJournalSchemaVersion = 1U;
constexpr size_t kSdJournalHeaderBytes = 72U;
constexpr size_t kSdJournalMaxPayloadBytes = 1024U;
constexpr size_t kSdJournalMaxRecordBytes =
    kSdJournalHeaderBytes + kSdJournalMaxPayloadBytes;

enum class SdJournalRecordType : uint8_t {
    Commit = 1,
    Tombstone = 2,
    Checkpoint = 3,
};

struct SdJournalRecordView {
    SdJournalRecordType type = SdJournalRecordType::Commit;
    uint64_t transaction_id = 0U;
    char dataset_id[kDatasetIdCharacters + 1U] = {};
    const uint8_t *payload = nullptr;
    size_t payload_size = 0U;
    size_t encoded_size = 0U;
    uint32_t payload_crc32 = 0U;
    uint32_t frame_crc32 = 0U;
};

enum class SdJournalDecodeStatus : uint8_t {
    Ok = 0,
    Incomplete,
    Corrupt,
    UnsupportedSchema,
    UnsupportedRecordType,
    DatasetMismatch,
    InvalidArgument,
};

enum class SdJournalScanStatus : uint8_t {
    Ok = 0,
    TailTruncated,
    Corrupt,
    UnsupportedSchema,
    UnsupportedRecordType,
    DatasetMismatch,
    NonMonotonicTransaction,
    ConsumerRejected,
    InvalidArgument,
};

struct SdJournalScanResult {
    SdJournalScanStatus status = SdJournalScanStatus::InvalidArgument;
    size_t valid_bytes = 0U;
    size_t tail_bytes = 0U;
    uint32_t record_count = 0U;
    uint64_t last_transaction_id = 0U;
};

using SdJournalRecordVisitor = bool (*)(void *context,
                                        const SdJournalRecordView &record);

// Encodes one append-only transaction frame. The payload may be arbitrary
// bounded bytes; its schema belongs to the owning domain. Zero means invalid
// input or insufficient output capacity.
size_t sd_journal_encode_record(SdJournalRecordType type,
                                const char *dataset_id,
                                uint64_t transaction_id,
                                const uint8_t *payload, size_t payload_size,
                                uint8_t *output, size_t output_capacity);

// Decodes exactly one frame from the supplied prefix. If a complete, valid
// header declares a payload that is not fully present, Incomplete is returned
// and encoded_size contains the bounded total frame size required by a
// streaming reader. Pointers in the view remain owned by the caller.
SdJournalDecodeStatus sd_journal_decode_record(
    const char *expected_dataset_id, const uint8_t *bytes, size_t size,
    SdJournalRecordView &record);

// Validates and replays complete frames in order without allocating. A final
// partial frame is reported as TailTruncated and is not delivered to the
// visitor. Any complete corrupt/unknown/mismatched frame blocks recovery.
SdJournalScanResult sd_journal_scan(const char *expected_dataset_id,
                                    const uint8_t *bytes, size_t size,
                                    SdJournalRecordVisitor visitor = nullptr,
                                    void *visitor_context = nullptr);

}  // namespace sticky_storage
