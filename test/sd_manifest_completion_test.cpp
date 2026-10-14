#include <array>
#include <cassert>
#include <cstdint>

#include "sd_manifest_completion.h"

int main()
{
    using namespace sticky_storage;

    SdManifestCompletionPool pool;
    SdManifestCompletionToken token = {};
    assert(!pool.reserve(0U, 1U, token));
    assert(!pool.reserve(1U, 0U, token));

    std::array<SdManifestCompletionToken, kStorageQueueDepth> tokens = {};
    for (size_t index = 0U; index < tokens.size(); ++index) {
        assert(pool.reserve(static_cast<uint32_t>(index + 1U), 4U,
                            tokens[index]));
        assert(tokens[index].id == index + 1U);
        assert(tokens[index].slot == index);
    }
    SdManifestCompletionToken overflow = {};
    assert(!pool.reserve(99U, 4U, overflow));
    assert(overflow.id == 0U);

    SdManifestCompletionRecord record = {};
    assert(pool.collect(tokens[0], record) ==
           SdManifestCompletionCollectStatus::Pending);

    SdManifestRequestResult success = {};
    success.status = SdManifestRequestStatus::Ok;
    success.changed = true;
    success.manifest.commit_generation = 2U;
    success.slot = SdManifestSlot::B;
    assert(pool.complete(tokens[0], 4U, success));
    assert(!pool.complete(tokens[0], 4U, success));
    assert(pool.collect(tokens[0], record) ==
           SdManifestCompletionCollectStatus::Ok);
    assert(record.request_id == tokens[0].id);
    assert(record.request_mount_generation == 4U);
    assert(record.observed_mount_generation == 4U);
    assert(record.outcome.status == SdManifestRequestStatus::Ok);
    assert(record.outcome.changed);
    assert(record.outcome.manifest.commit_generation == 2U);
    assert(record.outcome.slot == SdManifestSlot::B);
    assert(pool.collect(tokens[0], record) ==
           SdManifestCompletionCollectStatus::InvalidToken);

    SdManifestCompletionToken reused = {};
    assert(pool.reserve(100U, 7U, reused));
    assert(reused.slot == tokens[0].slot);
    assert(!pool.complete(tokens[0], 4U, success));
    assert(pool.collect(reused, record) ==
           SdManifestCompletionCollectStatus::Pending);
    assert(pool.cancel(reused));
    assert(!pool.cancel(reused));
    assert(pool.collect(reused, record) ==
           SdManifestCompletionCollectStatus::InvalidToken);

    SdManifestCompletionToken stale = {};
    assert(pool.reserve(101U, 7U, stale));
    SdManifestRequestResult stale_result = {};
    stale_result.status = SdManifestRequestStatus::StaleGeneration;
    assert(pool.complete(stale, 8U, stale_result));
    assert(pool.collect(stale, record) ==
           SdManifestCompletionCollectStatus::Ok);
    assert(record.request_mount_generation == 7U);
    assert(record.observed_mount_generation == 8U);
    assert(record.outcome.status == SdManifestRequestStatus::StaleGeneration);

    const SdManifestCompletionToken invalid_slot = {200U, 255U};
    assert(!pool.cancel(invalid_slot));
    assert(!pool.complete(invalid_slot, 1U, success));
    assert(pool.collect(invalid_slot, record) ==
           SdManifestCompletionCollectStatus::InvalidToken);

    for (size_t index = 1U; index < tokens.size(); ++index) {
        assert(pool.cancel(tokens[index]));
    }
    return 0;
}
