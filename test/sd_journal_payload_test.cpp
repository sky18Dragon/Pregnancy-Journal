#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>

#include "sd_journal_payload.h"

namespace {
constexpr char kId1[] = "00000000000000000000000000000001";
constexpr char kId2[] = "00000000000000000000000000000002";
}

int main()
{
    using namespace sticky_storage;
    SdJournalTransactionPayload transaction = {};
    transaction.mutation_count = 2U;
    assert(repository_key_make(RepositoryKind::Note, kId1,
                               transaction.mutations[0].key));
    transaction.mutations[0].type = RepositoryMutationType::Upsert;
    transaction.mutations[0].expected_revision = 0U;
    transaction.mutations[0].committed_revision = 1U;
    transaction.mutations[0].object_bytes = 123U;
    transaction.mutations[0].object_crc32 = 0x12345678U;
    std::strcpy(transaction.mutations[0].object_file, "OBJ00001.BIN");

    assert(repository_key_make(RepositoryKind::Weight, kId2,
                               transaction.mutations[1].key));
    transaction.mutations[1].type = RepositoryMutationType::Delete;
    transaction.mutations[1].expected_revision = 4U;
    transaction.mutations[1].committed_revision = 5U;

    std::array<uint8_t, kSdJournalMaxPayloadBytes> encoded = {};
    const size_t size = sd_journal_encode_transaction_payload(
        transaction, encoded.data(), encoded.size());
    assert(size == kSdJournalTransactionHeaderBytes +
                       2U * kSdJournalMutationBytes);
    SdJournalTransactionPayload decoded = {};
    assert(sd_journal_decode_transaction_payload(encoded.data(), size,
                                                 decoded) ==
           SdJournalPayloadStatus::Ok);
    assert(decoded.mutation_count == 2U);
    assert(decoded.mutations[0].type == RepositoryMutationType::Upsert);
    assert(decoded.mutations[0].object_bytes == 123U);
    assert(std::strcmp(decoded.mutations[0].object_file, "OBJ00001.BIN") ==
           0);
    assert(decoded.mutations[1].type == RepositoryMutationType::Delete);
    assert(decoded.mutations[1].committed_revision == 5U);
    assert(sd_journal_payload_valid_for_record(
        SdJournalRecordType::Commit, 9U, encoded.data(), size));
    assert(!sd_journal_payload_valid_for_record(
        SdJournalRecordType::Tombstone, 9U, encoded.data(), size));

    for (size_t partial = 0U; partial < size; ++partial) {
        const SdJournalPayloadStatus status =
            sd_journal_decode_transaction_payload(encoded.data(), partial,
                                                  decoded);
        assert(status == SdJournalPayloadStatus::Incomplete);
    }
    std::array<uint8_t, kSdJournalMaxPayloadBytes> corrupt = encoded;
    corrupt[4U] = 2U;
    assert(sd_journal_decode_transaction_payload(corrupt.data(), size,
                                                 decoded) ==
           SdJournalPayloadStatus::UnsupportedSchema);
    corrupt = encoded;
    corrupt[11U] = 1U;
    assert(sd_journal_decode_transaction_payload(corrupt.data(), size,
                                                 decoded) ==
           SdJournalPayloadStatus::Corrupt);
    corrupt = encoded;
    corrupt[16U] = 99U;
    assert(sd_journal_decode_transaction_payload(corrupt.data(), size,
                                                 decoded) ==
           SdJournalPayloadStatus::Corrupt);
    corrupt = encoded;
    corrupt[16U + 52U] = '.';
    assert(sd_journal_decode_transaction_payload(corrupt.data(), size,
                                                 decoded) ==
           SdJournalPayloadStatus::Corrupt);
    assert(sd_journal_decode_transaction_payload(encoded.data(), size + 1U,
                                                 decoded) ==
           SdJournalPayloadStatus::Corrupt);

    SdJournalCheckpointPayload checkpoint = {};
    checkpoint.last_transaction_id = 127U;
    checkpoint.segment_number = 2U;
    checkpoint.live_record_count = 17U;
    checkpoint.index_bytes = 4096U;
    checkpoint.index_crc32 = 0x87654321U;
    std::strcpy(checkpoint.index_file, "INDEX001.BIN");
    assert(sd_journal_encode_checkpoint_payload(
               checkpoint, encoded.data(), encoded.size()) ==
           kSdJournalCheckpointBytes);
    SdJournalCheckpointPayload decoded_checkpoint = {};
    assert(sd_journal_decode_checkpoint_payload(
               encoded.data(), kSdJournalCheckpointBytes,
               decoded_checkpoint) == SdJournalPayloadStatus::Ok);
    assert(decoded_checkpoint.last_transaction_id == 127U);
    assert(decoded_checkpoint.segment_number == 2U);
    assert(std::strcmp(decoded_checkpoint.index_file, "INDEX001.BIN") == 0);
    assert(sd_journal_payload_valid_for_record(
        SdJournalRecordType::Checkpoint, 128U, encoded.data(),
        kSdJournalCheckpointBytes));
    assert(!sd_journal_payload_valid_for_record(
        SdJournalRecordType::Checkpoint, 127U, encoded.data(),
        kSdJournalCheckpointBytes));
    encoded[4U] = 2U;
    assert(sd_journal_decode_checkpoint_payload(
               encoded.data(), kSdJournalCheckpointBytes,
               decoded_checkpoint) ==
           SdJournalPayloadStatus::UnsupportedSchema);

    SdJournalTransactionPayload deletion = {};
    deletion.mutation_count = 1U;
    assert(repository_key_make(RepositoryKind::Note, kId1,
                               deletion.mutations[0].key));
    deletion.mutations[0].type = RepositoryMutationType::Delete;
    deletion.mutations[0].expected_revision = 1U;
    deletion.mutations[0].committed_revision = 2U;
    const size_t deletion_size = sd_journal_encode_transaction_payload(
        deletion, encoded.data(), encoded.size());
    assert(deletion_size != 0U);
    assert(sd_journal_payload_valid_for_record(
        SdJournalRecordType::Tombstone, 129U, encoded.data(),
        deletion_size));
    assert(!sd_journal_payload_valid_for_record(
        SdJournalRecordType::Commit, 129U, encoded.data(), deletion_size));
    return 0;
}
