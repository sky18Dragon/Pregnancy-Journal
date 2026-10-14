#include <cassert>

#include "sd_repository_completion.h"

int main()
{
    using namespace sticky_storage;
    SdRepositoryCompletionPool pool;
    SdRepositoryRequest request = {};
    request.mount_generation = 3U;
    request.operation = SdRepositoryRequestOperation::Read;
    SdRepositoryCompletionToken first = {};
    assert(pool.reserve(10U, request, first));
    const SdRepositoryRequest *pending = nullptr;
    assert(pool.pending_request(first, pending));
    assert(pending != nullptr && pending->mount_generation == 3U);
    SdRepositoryCompletionRecord record = {};
    assert(pool.collect(first, record) ==
           SdRepositoryCompletionCollectStatus::Pending);
    SdRepositoryRequestResult outcome = {};
    outcome.status = SdRepositoryRequestStatus::NotFound;
    assert(pool.complete(first, 4U, outcome));
    assert(!pool.pending_request(first, pending));
    assert(pool.collect(first, record) ==
           SdRepositoryCompletionCollectStatus::Ok);
    assert(record.request_id == 10U &&
           record.request_mount_generation == 3U &&
           record.observed_mount_generation == 4U);
    assert(record.outcome.status == SdRepositoryRequestStatus::NotFound);
    assert(pool.collect(first, record) ==
           SdRepositoryCompletionCollectStatus::InvalidToken);

    SdRepositoryCompletionToken tokens[kStorageQueueDepth] = {};
    for (size_t index = 0U; index < kStorageQueueDepth; ++index) {
        assert(pool.reserve(static_cast<uint32_t>(20U + index), request,
                            tokens[index]));
    }
    SdRepositoryCompletionToken overflow = {};
    assert(!pool.reserve(99U, request, overflow));
    assert(pool.cancel(tokens[2]));
    assert(!pool.complete(tokens[2], 3U, outcome));
    assert(pool.reserve(100U, request, overflow));
    assert(overflow.slot == tokens[2].slot);
    assert(!pool.cancel(tokens[2]));
    return 0;
}
