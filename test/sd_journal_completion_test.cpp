#include <array>
#include <cassert>
#include <cstdint>

#include "sd_journal_completion.h"

int main()
{
    using namespace sticky_storage;

    SdJournalCompletionPool pool;
    std::array<SdJournalCompletionToken, kStorageQueueDepth> tokens = {};
    for (uint32_t index = 0U; index < tokens.size(); ++index) {
        assert(pool.reserve(index + 1U, 10U + index, tokens[index]));
        assert(tokens[index].id == index + 1U);
    }
    SdJournalCompletionToken overflow = {};
    assert(!pool.reserve(99U, 99U, overflow));

    SdJournalCompletionRecord record = {};
    assert(pool.collect(tokens[0], record) ==
           SdJournalCompletionCollectStatus::Pending);

    SdJournalRequestResult outcome = {};
    outcome.status = SdJournalRequestStatus::Ok;
    outcome.changed = true;
    outcome.probe.status = SdJournalStoreStatus::Ready;
    outcome.probe.active.last.transaction_id = 4U;
    assert(pool.complete(tokens[0], 10U, outcome));
    assert(!pool.complete(tokens[0], 10U, outcome));
    assert(pool.collect(tokens[0], record) ==
           SdJournalCompletionCollectStatus::Ok);
    assert(record.request_id == 1U);
    assert(record.request_mount_generation == 10U);
    assert(record.observed_mount_generation == 10U);
    assert(record.outcome.changed);
    assert(record.outcome.probe.active.last.transaction_id == 4U);
    assert(pool.collect(tokens[0], record) ==
           SdJournalCompletionCollectStatus::InvalidToken);

    SdJournalCompletionToken reused = {};
    assert(pool.reserve(100U, 20U, reused));
    assert(reused.slot == tokens[0].slot);
    assert(!pool.complete(tokens[0], 10U, outcome));
    assert(pool.complete(reused, 21U, outcome));
    assert(pool.collect(reused, record) ==
           SdJournalCompletionCollectStatus::Ok);
    assert(record.request_mount_generation == 20U);
    assert(record.observed_mount_generation == 21U);

    assert(pool.cancel(tokens[1]));
    assert(!pool.cancel(tokens[1]));
    assert(!pool.reserve(0U, 1U, overflow));
    assert(!pool.reserve(101U, 0U, overflow));
    return 0;
}
