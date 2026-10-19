#include "sd_repository_recovery.h"

#include <cstring>

#include "sd_storage_layout.h"
#include "storage_crc32.h"

namespace sticky_storage {
namespace {

bool key_less(const RepositoryKey &left, const RepositoryKey &right)
{
    if (left.repository != right.repository) {
        return static_cast<uint8_t>(left.repository) <
               static_cast<uint8_t>(right.repository);
    }
    return std::strcmp(left.record_id, right.record_id) < 0;
}

bool snapshot_entry_valid(const SdIndexSnapshotEntry &entry)
{
    if (!repository_key_valid(entry.key) || entry.revision == 0U) {
        return false;
    }
    if (entry.tombstone) {
        return entry.object_bytes == 0U && entry.object_crc32 == 0U &&
               entry.object_file[0] == '\0';
    }
    return entry.object_bytes > 0U &&
           entry.object_bytes <= kRepositoryPayloadBytes &&
           std::strlen(entry.object_file) <= kSdJournalObjectNameBytes &&
           fat83_name_valid(entry.object_file);
}

uint32_t frame_fingerprint(SdJournalRecordType type, uint64_t transaction_id,
                           const uint8_t *payload, size_t payload_size)
{
    uint32_t crc = kCrc32Initial;
    const uint8_t encoded_type = static_cast<uint8_t>(type);
    crc = crc32_update(crc, &encoded_type, sizeof(encoded_type));
    uint8_t transaction[8U] = {};
    for (size_t index = 0U; index < sizeof(transaction); ++index) {
        transaction[index] = static_cast<uint8_t>(
            (transaction_id >> (index * 8U)) & 0xFFU);
    }
    crc = crc32_update(crc, transaction, sizeof(transaction));
    crc = crc32_update(crc, payload, payload_size);
    return crc ^ kCrc32FinalXor;
}

}  // namespace

void SdRepositoryRecoveryIndex::reset()
{
    slots_.fill(Slot{});
    stored_count_ = 0U;
    live_count_ = 0U;
    last_transaction_id_ = 0U;
    last_fingerprint_ = 0U;
}

SdRepositoryRecoveryStatus SdRepositoryRecoveryIndex::restore_snapshot(
    const SdIndexSnapshot &snapshot)
{
    if (!sd_dataset_id_valid(snapshot.dataset_id) ||
        snapshot.last_transaction_id == 0U ||
        snapshot.entry_count > slots_.size()) {
        return SdRepositoryRecoveryStatus::InvalidArgument;
    }
    for (size_t index = 0U; index < snapshot.entry_count; ++index) {
        if (!snapshot_entry_valid(snapshot.entries[index]) ||
            (index != 0U &&
             !key_less(snapshot.entries[index - 1U].key,
                       snapshot.entries[index].key))) {
            return SdRepositoryRecoveryStatus::InvalidArgument;
        }
    }

    reset();
    for (size_t index = 0U; index < snapshot.entry_count; ++index) {
        const SdIndexSnapshotEntry &source = snapshot.entries[index];
        Slot &target = slots_[index];
        target.occupied = true;
        target.record.key = source.key;
        target.record.revision = source.revision;
        target.record.tombstone = source.tombstone;
        target.record.object_bytes = source.object_bytes;
        target.record.object_crc32 = source.object_crc32;
        std::memcpy(target.record.object_file, source.object_file,
                    sizeof(target.record.object_file));
        ++stored_count_;
        live_count_ += source.tombstone ? 0U : 1U;
    }
    last_transaction_id_ = snapshot.last_transaction_id;
    return SdRepositoryRecoveryStatus::Ok;
}

bool SdRepositoryRecoveryIndex::same_key(const RepositoryKey &left,
                                         const RepositoryKey &right)
{
    return left.repository == right.repository &&
           std::strcmp(left.record_id, right.record_id) == 0;
}

int SdRepositoryRecoveryIndex::find_slot(const RepositoryKey &key) const
{
    for (size_t index = 0U; index < slots_.size(); ++index) {
        if (slots_[index].occupied &&
            same_key(slots_[index].record.key, key)) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

int SdRepositoryRecoveryIndex::find_free_slot(
    const std::array<bool, kSdRecoveryRecordCapacity> &reserved) const
{
    for (size_t index = 0U; index < slots_.size(); ++index) {
        if (!slots_[index].occupied && !reserved[index]) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

SdRepositoryRecoveryStatus SdRepositoryRecoveryIndex::apply(
    SdJournalRecordType type, uint64_t transaction_id,
    const uint8_t *payload, size_t payload_size,
    SdRepositoryRecoveryResult &result)
{
    result = {};
    if (transaction_id == 0U ||
        (payload == nullptr && payload_size != 0U)) {
        return SdRepositoryRecoveryStatus::InvalidArgument;
    }
    if (!sd_journal_payload_valid_for_record(type, transaction_id, payload,
                                             payload_size)) {
        return SdRepositoryRecoveryStatus::UnsupportedPayload;
    }

    const uint32_t fingerprint =
        frame_fingerprint(type, transaction_id, payload, payload_size);
    if (transaction_id == last_transaction_id_) {
        if (fingerprint != last_fingerprint_) {
            return SdRepositoryRecoveryStatus::TransactionConflict;
        }
        result.transaction_id = transaction_id;
        return SdRepositoryRecoveryStatus::Ok;
    }
    if (transaction_id < last_transaction_id_) {
        return SdRepositoryRecoveryStatus::NonMonotonicTransaction;
    }

    if (type == SdJournalRecordType::Checkpoint) {
        SdJournalCheckpointPayload checkpoint = {};
        if (sd_journal_decode_checkpoint_payload(payload, payload_size,
                                                 checkpoint) !=
                SdJournalPayloadStatus::Ok ||
            checkpoint.last_transaction_id != last_transaction_id_ ||
            checkpoint.live_record_count != live_count_) {
            return SdRepositoryRecoveryStatus::CheckpointMismatch;
        }
        last_transaction_id_ = transaction_id;
        last_fingerprint_ = fingerprint;
        result.changed = true;
        result.transaction_id = transaction_id;
        return SdRepositoryRecoveryStatus::Ok;
    }

    SdJournalTransactionPayload transaction = {};
    if (sd_journal_decode_transaction_payload(payload, payload_size,
                                              transaction) !=
        SdJournalPayloadStatus::Ok) {
        return SdRepositoryRecoveryStatus::UnsupportedPayload;
    }

    std::array<int, kRepositoryBatchMutations> targets = {};
    std::array<bool, kSdRecoveryRecordCapacity> reserved = {};
    size_t new_records = 0U;
    for (size_t index = 0U; index < transaction.mutation_count; ++index) {
        const SdJournalMutationReference &mutation =
            transaction.mutations[index];
        for (size_t prior = 0U; prior < index; ++prior) {
            if (same_key(mutation.key,
                         transaction.mutations[prior].key)) {
                return SdRepositoryRecoveryStatus::InvalidArgument;
            }
        }

        int slot = find_slot(mutation.key);
        if (slot < 0) {
            if (mutation.type == RepositoryMutationType::Delete) {
                return SdRepositoryRecoveryStatus::NotFound;
            }
            if (mutation.expected_revision != 0U) {
                return SdRepositoryRecoveryStatus::RevisionConflict;
            }
            slot = find_free_slot(reserved);
            if (slot < 0) {
                return SdRepositoryRecoveryStatus::CapacityReached;
            }
            reserved[static_cast<size_t>(slot)] = true;
            ++new_records;
        } else {
            const SdRecoveredRecord &existing =
                slots_[static_cast<size_t>(slot)].record;
            if (existing.tombstone) {
                return SdRepositoryRecoveryStatus::Tombstoned;
            }
            if (existing.revision != mutation.expected_revision) {
                return SdRepositoryRecoveryStatus::RevisionConflict;
            }
        }
        targets[index] = slot;
    }
    if (new_records > slots_.size() - stored_count_) {
        return SdRepositoryRecoveryStatus::CapacityReached;
    }

    for (size_t index = 0U; index < transaction.mutation_count; ++index) {
        const SdJournalMutationReference &mutation =
            transaction.mutations[index];
        Slot &slot = slots_[static_cast<size_t>(targets[index])];
        if (!slot.occupied) {
            slot.occupied = true;
            slot.record.key = mutation.key;
            ++stored_count_;
            ++live_count_;
        }
        slot.record.revision = mutation.committed_revision;
        slot.record.tombstone =
            mutation.type == RepositoryMutationType::Delete;
        slot.record.object_bytes = mutation.object_bytes;
        slot.record.object_crc32 = mutation.object_crc32;
        std::memcpy(slot.record.object_file, mutation.object_file,
                    sizeof(slot.record.object_file));
        if (slot.record.tombstone) {
            if (live_count_ > 0U) {
                --live_count_;
            }
            slot.record.object_file[0] = '\0';
        }
    }

    last_transaction_id_ = transaction_id;
    last_fingerprint_ = fingerprint;
    result.changed = true;
    result.transaction_id = transaction_id;
    result.mutation_count = transaction.mutation_count;
    return SdRepositoryRecoveryStatus::Ok;
}

SdRepositoryRecoveryStatus SdRepositoryRecoveryIndex::read(
    const RepositoryKey &key, SdRecoveredRecord &record) const
{
    record = {};
    if (!repository_key_valid(key)) {
        return SdRepositoryRecoveryStatus::InvalidArgument;
    }
    const int slot = find_slot(key);
    if (slot < 0) {
        return SdRepositoryRecoveryStatus::NotFound;
    }
    record = slots_[static_cast<size_t>(slot)].record;
    return record.tombstone ? SdRepositoryRecoveryStatus::Tombstoned
                            : SdRepositoryRecoveryStatus::Ok;
}

RepositoryStatus SdRepositoryRecoveryIndex::list(
    const RepositoryPageRequest &request, RepositoryPage &page) const
{
    page = {};
    if (!repository_kind_valid(request.repository) || request.limit == 0U ||
        request.limit > page.entries.size() || request.cursor > slots_.size()) {
        return RepositoryStatus::InvalidArgument;
    }
    size_t index = request.cursor;
    for (; index < slots_.size() && page.count < request.limit; ++index) {
        const Slot &slot = slots_[index];
        if (!slot.occupied ||
            (slot.record.tombstone && !request.include_tombstones) ||
            slot.record.key.repository != request.repository) {
            continue;
        }
        RepositoryPageEntry &entry = page.entries[page.count++];
        std::memcpy(entry.record_id, slot.record.key.record_id,
                    sizeof(entry.record_id));
        entry.revision = slot.record.revision;
        entry.tombstone = slot.record.tombstone;
    }
    for (size_t remaining = index; remaining < slots_.size(); ++remaining) {
        const Slot &slot = slots_[remaining];
        if (slot.occupied &&
            (request.include_tombstones || !slot.record.tombstone) &&
            slot.record.key.repository == request.repository) {
            page.has_more = true;
            page.next_cursor = static_cast<uint32_t>(index);
            break;
        }
    }
    return RepositoryStatus::Ok;
}

bool SdRepositoryRecoveryIndex::record_at(size_t slot,
                                          SdRecoveredRecord &record) const
{
    record = {};
    if (slot >= slots_.size() || !slots_[slot].occupied) {
        return false;
    }
    record = slots_[slot].record;
    return true;
}

size_t SdRepositoryRecoveryIndex::stored_count() const
{
    return stored_count_;
}

size_t SdRepositoryRecoveryIndex::live_count() const { return live_count_; }

uint64_t SdRepositoryRecoveryIndex::last_transaction_id() const
{
    return last_transaction_id_;
}

}  // namespace sticky_storage
