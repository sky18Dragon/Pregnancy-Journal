#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "sd_journal_request.h"
#include "storage_contract.h"

namespace sticky_storage {

struct SdJournalCompletionToken {
    uint32_t id = 0U;
    uint8_t slot = 0U;
};

struct SdJournalCompletionRecord {
    uint32_t request_id = 0U;
    uint32_t request_mount_generation = 0U;
    uint32_t observed_mount_generation = 0U;
    SdJournalRequestResult outcome = {};
};

enum class SdJournalCompletionCollectStatus : uint8_t {
    Ok = 0,
    Pending,
    InvalidToken,
    InvalidState,
};

class SdJournalCompletionPool {
  public:
    bool reserve(uint32_t request_id, uint32_t request_mount_generation,
                 SdJournalCompletionToken &token);
    bool cancel(const SdJournalCompletionToken &token);
    bool complete(const SdJournalCompletionToken &token,
                  uint32_t observed_mount_generation,
                  const SdJournalRequestResult &outcome);
    SdJournalCompletionCollectStatus collect(
        const SdJournalCompletionToken &token,
        SdJournalCompletionRecord &record);

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
        SdJournalRequestResult outcome = {};
    };

    static void release(Entry &entry);

    std::array<Entry, kStorageQueueDepth> entries_ = {};
};

}  // namespace sticky_storage
