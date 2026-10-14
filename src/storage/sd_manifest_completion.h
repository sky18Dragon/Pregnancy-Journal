#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "sd_manifest_request.h"
#include "storage_contract.h"

namespace sticky_storage {

struct SdManifestCompletionToken {
    uint32_t id = 0U;
    uint8_t slot = 0U;
};

struct SdManifestCompletionRecord {
    uint32_t request_id = 0U;
    uint32_t request_mount_generation = 0U;
    uint32_t observed_mount_generation = 0U;
    SdManifestRequestResult outcome = {};
};

enum class SdManifestCompletionCollectStatus : uint8_t {
    Ok = 0,
    Pending,
    InvalidToken,
    InvalidState,
};

// Fixed-capacity result ownership for the asynchronous storage worker. A slot
// remains reserved until its terminal result is collected or an unqueued
// submission is cancelled. Request IDs prevent a delayed producer or stale
// consumer from touching a slot that has since been reused.
class SdManifestCompletionPool {
public:
    bool reserve(uint32_t request_id, uint32_t request_mount_generation,
                 SdManifestCompletionToken &token);
    bool cancel(const SdManifestCompletionToken &token);
    bool complete(const SdManifestCompletionToken &token,
                  uint32_t observed_mount_generation,
                  const SdManifestRequestResult &outcome);
    SdManifestCompletionCollectStatus collect(
        const SdManifestCompletionToken &token,
        SdManifestCompletionRecord &record);

    constexpr size_t capacity() const { return kStorageQueueDepth; }

private:
    enum class State : uint8_t {
        Free = 0,
        Reserving,
        Pending,
        Completing,
        Completed,
        Collecting,
    };

    struct Entry {
        std::atomic<uint8_t> state{static_cast<uint8_t>(State::Free)};
        std::atomic<uint32_t> id{0U};
        uint32_t request_mount_generation = 0U;
        uint32_t observed_mount_generation = 0U;
        SdManifestRequestResult outcome = {};
    };

    static void release(Entry &entry);

    std::array<Entry, kStorageQueueDepth> entries_ = {};
};

}  // namespace sticky_storage
