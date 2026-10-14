#pragma once

#include <cstddef>
#include <cstdint>

#include "sd_storage_service.h"

namespace sticky_storage {

enum class SdRepositoryAdapterStatus : uint8_t {
    Idle = 0,
    Pending,
    Ok,
    Busy,
    InvalidArgument,
    CardMissing,
    CardReadOnly,
    CardFull,
    StorageUnavailable,
    DatasetUnavailable,
    RecoveryUnavailable,
    StaleGeneration,
    NotFound,
    Tombstoned,
    RevisionConflict,
    TransactionConflict,
    CapacityReached,
    Corrupt,
    IoError,
};

SdRepositoryAdapterStatus sd_repository_adapter_preflight(
    const SdStorageServiceStatus &status, bool write);
SdRepositoryAdapterStatus sd_repository_adapter_map_result(
    SdRepositoryRequestStatus status);

// One adapter owns at most one request. Keep instances in static storage: its
// mutation and result workspaces intentionally stay off 4.6-5.1 KiB app task
// stacks. begin_* only queues work; poll() must be called by the app loop.
class SdRepositoryAdapter {
  public:
    SdRepositoryAdapterStatus begin_read(const RepositoryKey &key);
    SdRepositoryAdapterStatus begin_list(RepositoryKind repository,
                                         uint32_t cursor,
                                         bool include_tombstones = false);
    SdRepositoryAdapterStatus begin_upsert(
        const RepositoryKey &key, uint32_t expected_revision,
        const uint8_t *payload, size_t payload_size);
    SdRepositoryAdapterStatus begin_erase(const RepositoryKey &key,
                                          uint32_t expected_revision);
    SdRepositoryAdapterStatus poll();

    bool pending() const {
        return status_ == SdRepositoryAdapterStatus::Pending;
    }
    SdRepositoryAdapterStatus status() const { return status_; }
    const SdStorageRepositoryResult &result() const { return result_; }
    void reset();
    bool binding_current() const;

  private:
    SdRepositoryAdapterStatus begin_common(
        SdRepositoryRequestOperation operation, const RepositoryKey *key,
        const RepositoryPageRequest *page, uint32_t expected_revision,
        bool write);

    SdRepositoryAdapterStatus status_ = SdRepositoryAdapterStatus::Idle;
    uint32_t mount_generation_ = 0U;
    char dataset_id_[kDatasetIdCharacters + 1U] = {};
    SdStorageRequestHandle handle_ = {};
    RepositoryMutation mutation_ = {};
    SdStorageRepositoryResult result_ = {};
};

}  // namespace sticky_storage
