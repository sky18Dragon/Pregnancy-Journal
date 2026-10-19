#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>

#include "fake_storage_repository.h"

namespace {
constexpr char kId1[] = "00000000000000000000000000000001";
constexpr char kId2[] = "00000000000000000000000000000002";
constexpr char kId3[] = "00000000000000000000000000000003";
}

int main()
{
    using namespace sticky_storage;
    FakeStorageRepository storage;
    RepositoryKey first = {};
    RepositoryKey second = {};
    RepositoryKey third = {};
    assert(repository_key_make(RepositoryKind::Preference, kId1, first));
    assert(repository_key_make(RepositoryKind::Preference, kId2, second));
    assert(repository_key_make(RepositoryKind::Weight, kId3, third));
    RepositoryKey invalid = {};
    assert(!repository_key_make(RepositoryKind::Preference, "bad", invalid));

    RepositoryRecord record = {};
    assert(storage.read(first, record) == RepositoryStatus::NotFound);
    constexpr std::array<uint8_t, 3U> one = {1U, 2U, 3U};
    constexpr std::array<uint8_t, 2U> two = {4U, 5U};
    RepositoryMutation mutation = {};
    assert(repository_mutation_make_upsert(first, 0U, one.data(), one.size(),
                                           mutation));
    RepositoryBatchResult result = {};
    assert(storage.commit(mutation, 1U, result) == RepositoryStatus::Ok);
    assert(result.changed && result.revisions[0] == 1U);
    assert(storage.read(first, record) == RepositoryStatus::Ok);
    assert(record.revision == 1U && record.payload_size == one.size());
    assert(std::memcmp(record.payload.data(), one.data(), one.size()) == 0);

    RepositoryBatchResult retry = {};
    assert(storage.commit(mutation, 1U, retry) == RepositoryStatus::Ok);
    assert(!retry.changed && retry.revisions[0] == 1U);
    mutation.payload[0] ^= 0x40U;
    assert(storage.commit(mutation, 1U, result) ==
           RepositoryStatus::TransactionConflict);

    assert(repository_mutation_make_upsert(first, 0U, two.data(), two.size(),
                                           mutation));
    assert(storage.commit(mutation, 2U, result) ==
           RepositoryStatus::RevisionConflict);
    assert(storage.read(first, record) == RepositoryStatus::Ok);
    assert(record.revision == 1U);

    RepositoryBatch batch = {};
    batch.transaction_id = 3U;
    batch.mutation_count = 2U;
    assert(repository_mutation_make_upsert(first, 1U, two.data(), two.size(),
                                           batch.mutations[0]));
    assert(repository_mutation_make_upsert(second, 0U, one.data(), one.size(),
                                           batch.mutations[1]));
    assert(storage.commit_batch(batch, result) == RepositoryStatus::Ok);
    assert(result.changed && result.mutation_count == 2U);
    assert(result.revisions[0] == 2U && result.revisions[1] == 1U);
    assert(storage.read(first, record) == RepositoryStatus::Ok);
    assert(record.revision == 2U && record.payload_size == two.size());
    assert(storage.read(second, record) == RepositoryStatus::Ok);
    assert(record.revision == 1U);

    RepositoryPageRequest page_request = {};
    page_request.repository = RepositoryKind::Preference;
    page_request.limit = 1U;
    RepositoryPage page = {};
    assert(storage.list(page_request, page) == RepositoryStatus::Ok);
    assert(page.count == 1U && page.has_more && page.next_cursor != 0U);
    page_request.cursor = page.next_cursor;
    assert(storage.list(page_request, page) == RepositoryStatus::Ok);
    assert(page.count == 1U && !page.has_more);

    assert(storage.erase(first, 2U, 4U, result) == RepositoryStatus::Ok);
    assert(result.revisions[0] == 3U);
    assert(storage.read(first, record) == RepositoryStatus::Tombstoned);
    assert(record.revision == 3U && record.payload_size == 0U);
    page_request.cursor = 0U;
    page_request.limit = kRepositoryPageEntries;
    assert(storage.list(page_request, page) == RepositoryStatus::Ok);
    assert(page.count == 1U);
    assert(std::strcmp(page.entries[0].record_id, kId2) == 0);
    page_request.include_tombstones = true;
    assert(storage.list(page_request, page) == RepositoryStatus::Ok);
    assert(page.count == 2U);
    assert(std::strcmp(page.entries[0].record_id, kId1) == 0);
    assert(page.entries[0].tombstone && page.entries[0].revision == 3U);
    assert(std::strcmp(page.entries[1].record_id, kId2) == 0);
    assert(!page.entries[1].tombstone);

    assert(repository_mutation_make_upsert(first, 3U, one.data(), one.size(),
                                           mutation));
    assert(storage.commit(mutation, 5U, result) ==
           RepositoryStatus::Tombstoned);
    assert(storage.erase(first, 3U, 5U, result) ==
           RepositoryStatus::Tombstoned);

    batch = {};
    batch.transaction_id = 6U;
    batch.mutation_count = 2U;
    assert(repository_mutation_make_upsert(second, 1U, two.data(), two.size(),
                                           batch.mutations[0]));
    assert(repository_mutation_make_upsert(third, 1U, one.data(), one.size(),
                                           batch.mutations[1]));
    assert(storage.commit_batch(batch, result) ==
           RepositoryStatus::RevisionConflict);
    assert(storage.read(second, record) == RepositoryStatus::Ok);
    assert(record.revision == 1U);  // Batch failure changed nothing.
    assert(storage.read(third, record) == RepositoryStatus::NotFound);

    batch.mutations[1] = batch.mutations[0];
    assert(storage.commit_batch(batch, result) ==
           RepositoryStatus::InvalidArgument);
    assert(storage.last_transaction_id() == 4U);
    return 0;
}
