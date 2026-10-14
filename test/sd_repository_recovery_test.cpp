#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

#include "sd_repository_recovery.h"

namespace {
using namespace sticky_storage;

RepositoryKey key(RepositoryKind kind, unsigned id)
{
    char record_id[kRepositoryRecordIdCharacters + 1U] = {};
    std::snprintf(record_id, sizeof(record_id), "%032x", id);
    RepositoryKey value = {};
    assert(repository_key_make(kind, record_id, value));
    return value;
}

SdJournalMutationReference upsert(const RepositoryKey &record,
                                  uint32_t expected_revision,
                                  const char *file)
{
    SdJournalMutationReference mutation = {};
    mutation.type = RepositoryMutationType::Upsert;
    mutation.key = record;
    mutation.expected_revision = expected_revision;
    mutation.committed_revision = expected_revision + 1U;
    mutation.object_bytes = 10U + expected_revision;
    mutation.object_crc32 = 0x12345678U + expected_revision;
    std::strcpy(mutation.object_file, file);
    return mutation;
}

SdJournalMutationReference erase(const RepositoryKey &record,
                                 uint32_t expected_revision)
{
    SdJournalMutationReference mutation = {};
    mutation.type = RepositoryMutationType::Delete;
    mutation.key = record;
    mutation.expected_revision = expected_revision;
    mutation.committed_revision = expected_revision + 1U;
    return mutation;
}

size_t encode(const SdJournalTransactionPayload &transaction,
              std::array<uint8_t, kSdJournalMaxPayloadBytes> &bytes)
{
    return sd_journal_encode_transaction_payload(
        transaction, bytes.data(), bytes.size());
}

}  // namespace

int main()
{
    using namespace sticky_storage;
    SdRepositoryRecoveryIndex index;
    SdRepositoryRecoveryResult result = {};
    SdRecoveredRecord record = {};
    const RepositoryKey note = key(RepositoryKind::Note, 1U);
    const RepositoryKey weight = key(RepositoryKind::Weight, 2U);

    SdJournalTransactionPayload first = {};
    first.mutation_count = 2U;
    first.mutations[0] = upsert(note, 0U, "OBJ00001.BIN");
    first.mutations[1] = upsert(weight, 0U, "OBJ00002.BIN");
    std::array<uint8_t, kSdJournalMaxPayloadBytes> first_bytes = {};
    const size_t first_size = encode(first, first_bytes);
    assert(first_size != 0U);

    // Object files written before the journal commit are not visible until the
    // complete verified payload is replayed.
    assert(index.read(note, record) == SdRepositoryRecoveryStatus::NotFound);
    assert(index.apply(SdJournalRecordType::Commit, 1U, first_bytes.data(),
                       first_size, result) ==
           SdRepositoryRecoveryStatus::Ok);
    assert(result.changed && result.mutation_count == 2U);
    assert(index.stored_count() == 2U && index.live_count() == 2U);
    assert(index.read(note, record) == SdRepositoryRecoveryStatus::Ok);
    assert(record.revision == 1U &&
           std::strcmp(record.object_file, "OBJ00001.BIN") == 0);

    // Replaying the exact last frame is idempotent; the same ID with different
    // bytes is a conflict and an older frame never rolls state back.
    assert(index.apply(SdJournalRecordType::Commit, 1U, first_bytes.data(),
                       first_size, result) ==
           SdRepositoryRecoveryStatus::Ok);
    assert(!result.changed);
    SdJournalTransactionPayload conflicting = first;
    ++conflicting.mutations[0].object_crc32;
    std::array<uint8_t, kSdJournalMaxPayloadBytes> conflicting_bytes = {};
    const size_t conflicting_size = encode(conflicting, conflicting_bytes);
    assert(index.apply(SdJournalRecordType::Commit, 1U,
                       conflicting_bytes.data(), conflicting_size, result) ==
           SdRepositoryRecoveryStatus::TransactionConflict);

    // Validate the complete batch before mutating: the first update is valid,
    // but the second stale revision rejects everything.
    SdJournalTransactionPayload atomic = {};
    atomic.mutation_count = 2U;
    atomic.mutations[0] = upsert(note, 1U, "OBJ00003.BIN");
    atomic.mutations[1] = upsert(weight, 7U, "OBJ00004.BIN");
    std::array<uint8_t, kSdJournalMaxPayloadBytes> atomic_bytes = {};
    const size_t atomic_size = encode(atomic, atomic_bytes);
    assert(atomic_size != 0U);
    assert(index.apply(SdJournalRecordType::Commit, 2U,
                       atomic_bytes.data(), atomic_size, result) ==
           SdRepositoryRecoveryStatus::RevisionConflict);
    assert(index.read(note, record) == SdRepositoryRecoveryStatus::Ok);
    assert(record.revision == 1U &&
           std::strcmp(record.object_file, "OBJ00001.BIN") == 0);
    assert(index.last_transaction_id() == 1U);

    SdJournalTransactionPayload deletion = {};
    deletion.mutation_count = 1U;
    deletion.mutations[0] = erase(note, 1U);
    std::array<uint8_t, kSdJournalMaxPayloadBytes> delete_bytes = {};
    const size_t delete_size = encode(deletion, delete_bytes);
    assert(index.apply(SdJournalRecordType::Tombstone, 2U,
                       delete_bytes.data(), delete_size, result) ==
           SdRepositoryRecoveryStatus::Ok);
    assert(index.read(note, record) == SdRepositoryRecoveryStatus::Tombstoned);
    assert(index.live_count() == 1U);

    // A later cloud result cannot resurrect a tombstoned task/record ID.
    SdJournalTransactionPayload late = {};
    late.mutation_count = 1U;
    late.mutations[0] = upsert(note, 2U, "OBJ00005.BIN");
    std::array<uint8_t, kSdJournalMaxPayloadBytes> late_bytes = {};
    const size_t late_size = encode(late, late_bytes);
    assert(index.apply(SdJournalRecordType::Commit, 3U, late_bytes.data(),
                       late_size, result) ==
           SdRepositoryRecoveryStatus::Tombstoned);
    assert(index.last_transaction_id() == 2U);

    SdJournalCheckpointPayload checkpoint = {};
    checkpoint.last_transaction_id = 2U;
    checkpoint.segment_number = 1U;
    checkpoint.live_record_count = 99U;
    checkpoint.index_bytes = 64U;
    checkpoint.index_crc32 = 0x01020304U;
    std::strcpy(checkpoint.index_file, "INDEX001.BIN");
    std::array<uint8_t, kSdJournalMaxPayloadBytes> checkpoint_bytes = {};
    size_t checkpoint_size = sd_journal_encode_checkpoint_payload(
        checkpoint, checkpoint_bytes.data(), checkpoint_bytes.size());
    assert(index.apply(SdJournalRecordType::Checkpoint, 3U,
                       checkpoint_bytes.data(), checkpoint_size, result) ==
           SdRepositoryRecoveryStatus::CheckpointMismatch);
    assert(index.last_transaction_id() == 2U);
    checkpoint.last_transaction_id = 1U;
    checkpoint.live_record_count = 1U;
    checkpoint_size = sd_journal_encode_checkpoint_payload(
        checkpoint, checkpoint_bytes.data(), checkpoint_bytes.size());
    assert(index.apply(SdJournalRecordType::Checkpoint, 3U,
                       checkpoint_bytes.data(), checkpoint_size, result) ==
           SdRepositoryRecoveryStatus::CheckpointMismatch);
    checkpoint.last_transaction_id = 2U;
    checkpoint.live_record_count = 1U;
    checkpoint_size = sd_journal_encode_checkpoint_payload(
        checkpoint, checkpoint_bytes.data(), checkpoint_bytes.size());
    assert(index.apply(SdJournalRecordType::Checkpoint, 3U,
                       checkpoint_bytes.data(), checkpoint_size, result) ==
           SdRepositoryRecoveryStatus::Ok);
    assert(index.last_transaction_id() == 3U && result.changed);

    RepositoryPageRequest request = {};
    request.repository = RepositoryKind::Weight;
    request.limit = 1U;
    RepositoryPage page = {};
    assert(index.list(request, page) == RepositoryStatus::Ok);
    assert(page.count == 1U && page.entries[0].revision == 1U);

    // Capacity rejection also leaves the earlier committed view intact.
    SdRepositoryRecoveryIndex full;
    uint64_t transaction_id = 1U;
    for (size_t item = 0U; item < kSdRecoveryRecordCapacity; ++item) {
        SdJournalTransactionPayload addition = {};
        addition.mutation_count = 1U;
        char file[13U] = {};
        std::snprintf(file, sizeof(file), "O%07u.BIN",
                      static_cast<unsigned>(item));
        addition.mutations[0] = upsert(
            key(RepositoryKind::Note, static_cast<unsigned>(item + 1U)),
            0U, file);
        std::array<uint8_t, kSdJournalMaxPayloadBytes> bytes = {};
        const size_t size = encode(addition, bytes);
        assert(full.apply(SdJournalRecordType::Commit, transaction_id++,
                          bytes.data(), size, result) ==
               SdRepositoryRecoveryStatus::Ok);
    }
    SdJournalTransactionPayload overflow = {};
    overflow.mutation_count = 1U;
    overflow.mutations[0] = upsert(key(RepositoryKind::Note, kSdRecoveryRecordCapacity + 1U), 0U,
                                   "OVERFLOW.BIN");
    std::array<uint8_t, kSdJournalMaxPayloadBytes> overflow_bytes = {};
    const size_t overflow_size = encode(overflow, overflow_bytes);
    assert(full.apply(SdJournalRecordType::Commit, transaction_id,
                      overflow_bytes.data(), overflow_size, result) ==
           SdRepositoryRecoveryStatus::CapacityReached);
    assert(full.stored_count() == kSdRecoveryRecordCapacity);

    // Tombstones retain their slots: deleting never permits accidental ID reuse.
    SdJournalTransactionPayload removal = {};
    removal.mutation_count = 1U;
    removal.mutations[0].type = RepositoryMutationType::Delete;
    removal.mutations[0].key = key(RepositoryKind::Note, 1U);
    removal.mutations[0].expected_revision = 1U;
    removal.mutations[0].committed_revision = 2U;
    std::array<uint8_t, kSdJournalMaxPayloadBytes> removal_bytes = {};
    const auto removal_size = encode(removal, removal_bytes);
    assert(full.apply(SdJournalRecordType::Tombstone, transaction_id++,
                       removal_bytes.data(), removal_size, result) == SdRepositoryRecoveryStatus::Ok);
    assert(full.live_count() == kSdRecoveryRecordCapacity - 1U);
    assert(full.stored_count() == kSdRecoveryRecordCapacity);
    assert(full.apply(SdJournalRecordType::Commit, transaction_id,
                       overflow_bytes.data(), overflow_size, result) == SdRepositoryRecoveryStatus::CapacityReached);
    request = {};
    request.repository = RepositoryKind::Note;
    request.include_tombstones = true;
    size_t listed = 0U;
    do {
        assert(full.list(request, page) == RepositoryStatus::Ok);
        listed += page.count;
        assert(!page.has_more || page.next_cursor > request.cursor);
        request.cursor = page.next_cursor;
    } while (page.has_more);
    assert(listed == kSdRecoveryRecordCapacity);
    return 0;
}
