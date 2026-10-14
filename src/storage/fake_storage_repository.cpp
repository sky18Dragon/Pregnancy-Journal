#include "fake_storage_repository.h"

#include <cstring>

#include "storage_crc32.h"

namespace sticky_storage {
namespace {

uint32_t crc_u32(uint32_t crc, uint32_t value)
{
    uint8_t bytes[4U] = {};
    for (size_t index = 0U; index < sizeof(bytes); ++index) {
        bytes[index] =
            static_cast<uint8_t>((value >> (index * 8U)) & 0xFFU);
    }
    return crc32_update(crc, bytes, sizeof(bytes));
}

uint32_t crc_u64(uint32_t crc, uint64_t value)
{
    uint8_t bytes[8U] = {};
    for (size_t index = 0U; index < sizeof(bytes); ++index) {
        bytes[index] =
            static_cast<uint8_t>((value >> (index * 8U)) & 0xFFU);
    }
    return crc32_update(crc, bytes, sizeof(bytes));
}

}  // namespace

bool FakeStorageRepository::same_key(const RepositoryKey &left,
                                     const RepositoryKey &right)
{
    return left.repository == right.repository &&
           std::strcmp(left.record_id, right.record_id) == 0;
}

uint32_t FakeStorageRepository::batch_fingerprint(
    const RepositoryBatch &batch)
{
    uint32_t crc = crc_u64(kCrc32Initial, batch.transaction_id);
    crc = crc_u32(crc, static_cast<uint32_t>(batch.mutation_count));
    for (size_t index = 0U; index < batch.mutation_count; ++index) {
        const RepositoryMutation &mutation = batch.mutations[index];
        const uint8_t type = static_cast<uint8_t>(mutation.type);
        const uint8_t repository =
            static_cast<uint8_t>(mutation.key.repository);
        crc = crc32_update(crc, &type, 1U);
        crc = crc32_update(crc, &repository, 1U);
        crc = crc32_update(
            crc, reinterpret_cast<const uint8_t *>(mutation.key.record_id),
            kRepositoryRecordIdCharacters);
        crc = crc_u32(crc, mutation.expected_revision);
        crc = crc_u32(crc, static_cast<uint32_t>(mutation.payload_size));
        crc = crc32_update(crc, mutation.payload.data(),
                           mutation.payload_size);
    }
    return crc ^ kCrc32FinalXor;
}

int FakeStorageRepository::find_slot(const RepositoryKey &key) const
{
    for (size_t index = 0U; index < slots_.size(); ++index) {
        if (slots_[index].occupied &&
            same_key(slots_[index].record.key, key)) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

int FakeStorageRepository::find_free_slot() const
{
    for (size_t index = 0U; index < slots_.size(); ++index) {
        if (!slots_[index].occupied) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

RepositoryStatus FakeStorageRepository::read(
    const RepositoryKey &key, RepositoryRecord &record) const
{
    record = {};
    if (!repository_key_valid(key)) {
        return RepositoryStatus::InvalidArgument;
    }
    const int slot = find_slot(key);
    if (slot < 0) {
        return RepositoryStatus::NotFound;
    }
    record = slots_[static_cast<size_t>(slot)].record;
    return record.tombstone ? RepositoryStatus::Tombstoned
                            : RepositoryStatus::Ok;
}

RepositoryStatus FakeStorageRepository::list(
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

RepositoryStatus FakeStorageRepository::commit(
    const RepositoryMutation &mutation, uint64_t transaction_id,
    RepositoryBatchResult &result)
{
    RepositoryBatch batch = {};
    batch.transaction_id = transaction_id;
    batch.mutation_count = 1U;
    batch.mutations[0] = mutation;
    return commit_batch(batch, result);
}

RepositoryStatus FakeStorageRepository::erase(
    const RepositoryKey &key, uint32_t expected_revision,
    uint64_t transaction_id, RepositoryBatchResult &result)
{
    RepositoryMutation mutation = {};
    if (!repository_mutation_make_delete(key, expected_revision, mutation)) {
        result = {};
        return RepositoryStatus::InvalidArgument;
    }
    return commit(mutation, transaction_id, result);
}

RepositoryStatus FakeStorageRepository::commit_batch(
    const RepositoryBatch &batch, RepositoryBatchResult &result)
{
    result = {};
    if (batch.transaction_id == 0U || batch.mutation_count == 0U ||
        batch.mutation_count > batch.mutations.size()) {
        return RepositoryStatus::InvalidArgument;
    }
    for (size_t index = 0U; index < batch.mutation_count; ++index) {
        const RepositoryMutation &mutation = batch.mutations[index];
        if (!repository_key_valid(mutation.key) ||
            (mutation.type != RepositoryMutationType::Upsert &&
             mutation.type != RepositoryMutationType::Delete) ||
            mutation.payload_size > mutation.payload.size() ||
            (mutation.type == RepositoryMutationType::Delete &&
             mutation.payload_size != 0U)) {
            return RepositoryStatus::InvalidArgument;
        }
        for (size_t prior = 0U; prior < index; ++prior) {
            if (same_key(mutation.key, batch.mutations[prior].key)) {
                return RepositoryStatus::InvalidArgument;
            }
        }
    }

    const uint32_t fingerprint = batch_fingerprint(batch);
    if (batch.transaction_id == last_transaction_id_) {
        if (fingerprint == last_transaction_fingerprint_) {
            result = last_result_;
            result.changed = false;
            return RepositoryStatus::Ok;
        }
        return RepositoryStatus::TransactionConflict;
    }
    if (batch.transaction_id < last_transaction_id_) {
        return RepositoryStatus::TransactionConflict;
    }

    size_t new_records = 0U;
    std::array<int, kRepositoryBatchMutations> target_slots = {};
    for (size_t index = 0U; index < batch.mutation_count; ++index) {
        const RepositoryMutation &mutation = batch.mutations[index];
        const int slot = find_slot(mutation.key);
        target_slots[index] = slot;
        if (slot < 0) {
            if (mutation.type == RepositoryMutationType::Delete) {
                return RepositoryStatus::NotFound;
            }
            if (mutation.expected_revision != 0U) {
                return RepositoryStatus::RevisionConflict;
            }
            ++new_records;
            continue;
        }
        const RepositoryRecord &existing =
            slots_[static_cast<size_t>(slot)].record;
        if (existing.tombstone) {
            return RepositoryStatus::Tombstoned;
        }
        if (existing.revision != mutation.expected_revision ||
            existing.revision == UINT32_MAX) {
            return RepositoryStatus::RevisionConflict;
        }
    }
    if (new_records > slots_.size() - stored_count_) {
        return RepositoryStatus::CapacityReached;
    }

    result.changed = true;
    result.transaction_id = batch.transaction_id;
    result.mutation_count = batch.mutation_count;
    for (size_t index = 0U; index < batch.mutation_count; ++index) {
        const RepositoryMutation &mutation = batch.mutations[index];
        int slot = target_slots[index];
        if (slot < 0) {
            slot = find_free_slot();
            if (slot < 0) {
                return RepositoryStatus::CapacityReached;
            }
            slots_[static_cast<size_t>(slot)].occupied = true;
            slots_[static_cast<size_t>(slot)].record.key = mutation.key;
            ++stored_count_;
        }
        RepositoryRecord &record = slots_[static_cast<size_t>(slot)].record;
        record.revision = mutation.expected_revision + 1U;
        record.tombstone =
            mutation.type == RepositoryMutationType::Delete;
        record.payload_size = record.tombstone ? 0U : mutation.payload_size;
        record.payload.fill(0U);
        if (record.payload_size != 0U) {
            std::memcpy(record.payload.data(), mutation.payload.data(),
                        record.payload_size);
        }
        result.revisions[index] = record.revision;
    }

    last_transaction_id_ = batch.transaction_id;
    last_transaction_fingerprint_ = fingerprint;
    last_result_ = result;
    return RepositoryStatus::Ok;
}

}  // namespace sticky_storage
