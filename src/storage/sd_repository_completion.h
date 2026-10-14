#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "sd_repository_request.h"
#include "storage_contract.h"

namespace sticky_storage {

struct SdRepositoryCompletionToken {
    uint32_t id = 0U;
    uint8_t slot = 0U;
};

struct SdRepositoryCompletionRecord {
    uint32_t request_id = 0U;
    uint32_t request_mount_generation = 0U;
    uint32_t observed_mount_generation = 0U;
    SdRepositoryRequestResult outcome = {};
};

enum class SdRepositoryCompletionCollectStatus : uint8_t {
    Ok = 0,
    Pending,
    InvalidToken,
    InvalidState,
};

class SdRepositoryCompletionPool {
  public:
    bool reserve(uint32_t request_id, const SdRepositoryRequest &request,
                 SdRepositoryCompletionToken &token);
    bool reserve_read(uint32_t request_id, uint32_t mount_generation,
                      const char *dataset_id, const RepositoryKey &key,
                      SdRepositoryCompletionToken &token);
    bool reserve_list(uint32_t request_id, uint32_t mount_generation,
                      const char *dataset_id,
                      const RepositoryPageRequest &page,
                      SdRepositoryCompletionToken &token);
    bool reserve_commit(uint32_t request_id, uint32_t mount_generation,
                        const char *dataset_id,
                        const RepositoryMutation &mutation,
                        uint64_t transaction_id,
                        SdRepositoryCompletionToken &token);
    bool reserve_erase(uint32_t request_id, uint32_t mount_generation,
                       const char *dataset_id, const RepositoryKey &key,
                       uint32_t expected_revision, uint64_t transaction_id,
                       SdRepositoryCompletionToken &token);
    bool cancel(const SdRepositoryCompletionToken &token);
    // The returned pointer remains owned by the pool and is valid only while
    // this token is Pending. Only the single storage worker may call this.
    bool pending_request(const SdRepositoryCompletionToken &token,
                         const SdRepositoryRequest *&request) const;
    bool complete(const SdRepositoryCompletionToken &token,
                  uint32_t observed_mount_generation,
                  const SdRepositoryRequestResult &outcome);
    SdRepositoryCompletionCollectStatus collect(
        const SdRepositoryCompletionToken &token,
        SdRepositoryCompletionRecord &record);
    // Stack-safe collection path used by application tasks: copies the large
    // outcome directly into caller-owned static storage.
    SdRepositoryCompletionCollectStatus collect(
        const SdRepositoryCompletionToken &token, uint32_t &request_id,
        uint32_t &request_mount_generation,
        uint32_t &observed_mount_generation,
        SdRepositoryRequestResult &outcome);

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
        SdRepositoryRequest request = {};
        uint32_t observed_mount_generation = 0U;
        SdRepositoryRequestResult outcome = {};
    };

    static void release(Entry &entry);
    Entry *acquire(uint32_t request_id,
                   SdRepositoryCompletionToken &token);
    bool publish(Entry &entry, const SdRepositoryCompletionToken &token,
                 bool request_valid);

    std::array<Entry, kStorageQueueDepth> entries_ = {};
};

}  // namespace sticky_storage
