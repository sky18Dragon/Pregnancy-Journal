#include "sd_repository_adapter.h"

#include <cstring>

namespace sticky_storage {

SdRepositoryAdapterStatus sd_repository_adapter_preflight(
    const SdStorageServiceStatus &status, bool write)
{
    if (!status.running || !status.accepting_repository_requests) {
        return SdRepositoryAdapterStatus::StorageUnavailable;
    }
    switch (status.card_state) {
    case SdCardState::Absent:
    case SdCardState::Unmounted:
        return SdRepositoryAdapterStatus::CardMissing;
    case SdCardState::ReadyReadOnly:
        if (write) return SdRepositoryAdapterStatus::CardReadOnly;
        break;
    case SdCardState::Full:
        if (write) return SdRepositoryAdapterStatus::CardFull;
        break;
    case SdCardState::ReadyReadWrite:
        break;
    case SdCardState::Mounting:
    case SdCardState::Fault:
    case SdCardState::Ejecting:
        return SdRepositoryAdapterStatus::StorageUnavailable;
    }
    if (!status.manifest_known || !status.manifest_selected ||
        (status.manifest_state != SdManifestProbeState::Ready &&
         status.manifest_state != SdManifestProbeState::Degraded)) {
        return SdRepositoryAdapterStatus::DatasetUnavailable;
    }
    if (!status.recovery_known || !status.recovery_view_ready) {
        return SdRepositoryAdapterStatus::RecoveryUnavailable;
    }
    if (write && !status.capacity_known) {
        return SdRepositoryAdapterStatus::StorageUnavailable;
    }
    return SdRepositoryAdapterStatus::Ok;
}

SdRepositoryAdapterStatus sd_repository_adapter_map_result(
    SdRepositoryRequestStatus status)
{
    switch (status) {
    case SdRepositoryRequestStatus::Ok:
        return SdRepositoryAdapterStatus::Ok;
    case SdRepositoryRequestStatus::InvalidArgument:
        return SdRepositoryAdapterStatus::InvalidArgument;
    case SdRepositoryRequestStatus::CardMissing:
        return SdRepositoryAdapterStatus::CardMissing;
    case SdRepositoryRequestStatus::CardReadOnly:
        return SdRepositoryAdapterStatus::CardReadOnly;
    case SdRepositoryRequestStatus::CardFull:
        return SdRepositoryAdapterStatus::CardFull;
    case SdRepositoryRequestStatus::CardUnavailable:
    case SdRepositoryRequestStatus::CapacityUnavailable:
        return SdRepositoryAdapterStatus::StorageUnavailable;
    case SdRepositoryRequestStatus::StaleGeneration:
    case SdRepositoryRequestStatus::DatasetMismatch:
        return SdRepositoryAdapterStatus::StaleGeneration;
    case SdRepositoryRequestStatus::ManifestUnavailable:
        return SdRepositoryAdapterStatus::DatasetUnavailable;
    case SdRepositoryRequestStatus::RecoveryUnavailable:
        return SdRepositoryAdapterStatus::RecoveryUnavailable;
    case SdRepositoryRequestStatus::NotFound:
        return SdRepositoryAdapterStatus::NotFound;
    case SdRepositoryRequestStatus::Tombstoned:
        return SdRepositoryAdapterStatus::Tombstoned;
    case SdRepositoryRequestStatus::RevisionConflict:
        return SdRepositoryAdapterStatus::RevisionConflict;
    case SdRepositoryRequestStatus::TransactionConflict:
        return SdRepositoryAdapterStatus::TransactionConflict;
    case SdRepositoryRequestStatus::CapacityReached:
        return SdRepositoryAdapterStatus::CapacityReached;
    case SdRepositoryRequestStatus::ObjectError:
    case SdRepositoryRequestStatus::RecoveryError:
        return SdRepositoryAdapterStatus::Corrupt;
    case SdRepositoryRequestStatus::JournalError:
        return SdRepositoryAdapterStatus::IoError;
    }
    return SdRepositoryAdapterStatus::IoError;
}

bool SdRepositoryAdapter::binding_current() const
{
    const auto current = sd_storage_service_status();
    return mount_generation_ != 0U && current.mount_generation == mount_generation_ &&
           current.manifest_selected &&
           std::strcmp(current.selected_manifest.dataset_id, dataset_id_) == 0 &&
           sd_repository_adapter_preflight(current, false) == SdRepositoryAdapterStatus::Ok;
}

void SdRepositoryAdapter::reset()
{
    mount_generation_ = 0U;
    dataset_id_[0] = '\0';
    if (pending()) return;
    status_ = SdRepositoryAdapterStatus::Idle;
    handle_ = {};
    mutation_ = {};
    result_ = {};
}

SdRepositoryAdapterStatus SdRepositoryAdapter::begin_common(
    SdRepositoryRequestOperation operation, const RepositoryKey *key,
    const RepositoryPageRequest *page, uint32_t expected_revision,
    bool write)
{
    if (pending()) return SdRepositoryAdapterStatus::Busy;
    handle_ = {};
    result_ = {};
    const SdStorageServiceStatus service = sd_storage_service_status();
    status_ = sd_repository_adapter_preflight(service, write);
    if (status_ != SdRepositoryAdapterStatus::Ok) return status_;
    if (mount_generation_ != 0U && !binding_current()) {
        status_ = SdRepositoryAdapterStatus::StaleGeneration;
        return status_;
    }
    mount_generation_ = service.mount_generation;
    std::memcpy(dataset_id_, service.selected_manifest.dataset_id, sizeof(dataset_id_));

    esp_err_t queued = ESP_ERR_INVALID_ARG;
    switch (operation) {
    case SdRepositoryRequestOperation::Read:
        if (key != nullptr) {
            queued = sd_storage_service_submit_repository_read(
                service.mount_generation,
                service.selected_manifest.dataset_id, *key, handle_);
        }
        break;
    case SdRepositoryRequestOperation::List:
        if (page != nullptr) {
            queued = sd_storage_service_submit_repository_list(
                service.mount_generation,
                service.selected_manifest.dataset_id, *page, handle_);
        }
        break;
    case SdRepositoryRequestOperation::Commit:
        queued = sd_storage_service_submit_repository_commit(
            service.mount_generation, service.selected_manifest.dataset_id,
            mutation_, kRepositoryAutoTransactionId, handle_);
        break;
    case SdRepositoryRequestOperation::Erase:
        if (key != nullptr) {
            queued = sd_storage_service_submit_repository_erase(
                service.mount_generation,
                service.selected_manifest.dataset_id, *key,
                expected_revision, kRepositoryAutoTransactionId, handle_);
        }
        break;
    }
    if (queued == ESP_OK) {
        status_ = SdRepositoryAdapterStatus::Pending;
    } else if (queued == ESP_ERR_INVALID_ARG) {
        status_ = SdRepositoryAdapterStatus::InvalidArgument;
    } else if (queued == ESP_ERR_NO_MEM || queued == ESP_ERR_TIMEOUT) {
        status_ = SdRepositoryAdapterStatus::Busy;
    } else {
        status_ = SdRepositoryAdapterStatus::StorageUnavailable;
    }
    return status_;
}

SdRepositoryAdapterStatus SdRepositoryAdapter::begin_read(
    const RepositoryKey &key)
{
    return begin_common(SdRepositoryRequestOperation::Read, &key, nullptr,
                        0U, false);
}

SdRepositoryAdapterStatus SdRepositoryAdapter::begin_list(
    RepositoryKind repository, uint32_t cursor, bool include_tombstones)
{
    RepositoryPageRequest page = {};
    page.repository = repository;
    page.cursor = cursor;
    page.limit = kRepositoryPageEntries;
    page.include_tombstones = include_tombstones;
    return begin_common(SdRepositoryRequestOperation::List, nullptr, &page,
                        0U, false);
}

SdRepositoryAdapterStatus SdRepositoryAdapter::begin_upsert(
    const RepositoryKey &key, uint32_t expected_revision,
    const uint8_t *payload, size_t payload_size)
{
    mutation_ = {};
    if (!repository_mutation_make_upsert(
            key, expected_revision, payload, payload_size, mutation_)) {
        status_ = SdRepositoryAdapterStatus::InvalidArgument;
        return status_;
    }
    return begin_common(SdRepositoryRequestOperation::Commit, nullptr,
                        nullptr, 0U, true);
}

SdRepositoryAdapterStatus SdRepositoryAdapter::begin_erase(
    const RepositoryKey &key, uint32_t expected_revision)
{
    return begin_common(SdRepositoryRequestOperation::Erase, &key, nullptr,
                        expected_revision, true);
}

SdRepositoryAdapterStatus SdRepositoryAdapter::poll()
{
    if (!pending()) return status_;
    const esp_err_t collected =
        sd_storage_service_collect_repository_result(handle_, result_);
    if (collected == ESP_ERR_NOT_FINISHED) return status_;
    handle_ = {};
    if (collected != ESP_OK) {
        status_ = collected == ESP_ERR_INVALID_ARG
                      ? SdRepositoryAdapterStatus::InvalidArgument
                      : SdRepositoryAdapterStatus::StorageUnavailable;
        return status_;
    }
    status_ = binding_current() ? sd_repository_adapter_map_result(result_.outcome.status)
                                : SdRepositoryAdapterStatus::StaleGeneration;
    return status_;
}

}  // namespace sticky_storage
