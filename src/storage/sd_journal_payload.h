#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "sd_journal.h"
#include "storage_repository.h"

namespace sticky_storage {

constexpr uint16_t kSdJournalPayloadSchemaVersion = 1U;
constexpr size_t kSdJournalTransactionHeaderBytes = 16U;
constexpr size_t kSdJournalMutationBytes = 64U;
constexpr size_t kSdJournalCheckpointBytes = 64U;
constexpr size_t kSdJournalObjectNameBytes = 12U;

struct SdJournalMutationReference {
    RepositoryMutationType type = RepositoryMutationType::Upsert;
    RepositoryKey key = {};
    uint32_t expected_revision = 0U;
    uint32_t committed_revision = 0U;
    uint32_t object_bytes = 0U;
    uint32_t object_crc32 = 0U;
    char object_file[kSdJournalObjectNameBytes + 1U] = {};
};

struct SdJournalTransactionPayload {
    std::array<SdJournalMutationReference, kRepositoryBatchMutations>
        mutations = {};
    size_t mutation_count = 0U;
};

struct SdJournalCheckpointPayload {
    uint64_t last_transaction_id = 0U;
    uint32_t segment_number = 0U;
    uint32_t live_record_count = 0U;
    uint32_t index_bytes = 0U;
    uint32_t index_crc32 = 0U;
    char index_file[kSdJournalObjectNameBytes + 1U] = {};
};

enum class SdJournalPayloadStatus : uint8_t {
    Ok = 0,
    InvalidArgument,
    Incomplete,
    Corrupt,
    UnsupportedSchema,
};

size_t sd_journal_encode_transaction_payload(
    const SdJournalTransactionPayload &payload, uint8_t *output,
    size_t output_capacity);
SdJournalPayloadStatus sd_journal_decode_transaction_payload(
    const uint8_t *bytes, size_t size, SdJournalTransactionPayload &payload);

size_t sd_journal_encode_checkpoint_payload(
    const SdJournalCheckpointPayload &payload, uint8_t *output,
    size_t output_capacity);
SdJournalPayloadStatus sd_journal_decode_checkpoint_payload(
    const uint8_t *bytes, size_t size, SdJournalCheckpointPayload &payload);

// Validates the domain payload carried by a journal frame. The low-level
// frame codec intentionally accepts opaque bytes; all worker append requests
// must pass this stricter contract before any file I/O is attempted.
bool sd_journal_payload_valid_for_record(SdJournalRecordType type,
                                         uint64_t transaction_id,
                                         const uint8_t *bytes, size_t size);

}  // namespace sticky_storage
