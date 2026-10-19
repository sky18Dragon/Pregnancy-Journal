#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "sd_storage_policy.h"
#include "storage_contract.h"

namespace sticky_storage {

enum class SdStorageWorkKind : uint8_t {
    Record = 0,
};

struct SdStorageWorkToken {
    uint32_t id = 0U;
    uint8_t slot = 0U;
};

struct SdStorageWorkRecord {
    uint32_t id = 0U;
    SdStorageWorkKind kind = SdStorageWorkKind::Record;
    SdDatasetBinding binding = {};
};

enum class SdStorageWorkStatus : uint8_t {
    Ok = 0,
    AdmissionClosed,
    CapacityReached,
    Busy,
    CardUnavailable,
    CardReadOnly,
    NoSpace,
    ManifestUnavailable,
    InvalidArgument,
    InvalidToken,
    StaleGeneration,
    DatasetMismatch,
};

// Bounded lease table used by record work that can outlive
// one storage-worker queue command. The service serializes calls with its
// submission mutex; the class itself intentionally owns no RTOS primitive.
class SdStorageWorkGate {
  public:
    SdStorageWorkStatus admit(SdStorageWorkKind kind,
                              const SdDatasetBinding &binding,
                              uint32_t current_mount_generation,
                              const char *current_dataset_id,
                              uint32_t request_id,
                              SdStorageWorkToken &token);
    SdStorageWorkStatus finish(const SdStorageWorkToken &token,
                               uint32_t current_mount_generation,
                               const char *current_dataset_id,
                               SdStorageWorkRecord &record);
    bool cancel(const SdStorageWorkToken &token,
                SdStorageWorkRecord &record);

    void close_admission();
    void open_admission();
    bool accepting() const;
    size_t pending() const;
    constexpr size_t capacity() const { return kStorageQueueDepth; }

  private:
    struct Entry {
        bool occupied = false;
        SdStorageWorkRecord record = {};
    };

    static bool kind_valid(SdStorageWorkKind kind);
    bool accepting_ = true;
    size_t pending_ = 0U;
    std::array<Entry, kStorageQueueDepth> entries_ = {};
};

}  // namespace sticky_storage
