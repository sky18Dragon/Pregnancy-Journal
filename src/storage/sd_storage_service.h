#pragma once

#include <cstdint>
#include <cstring>

#include "esp_err.h"
#include "sd_card_state.h"
#include "sd_journal_request.h"
#include "sd_index_snapshot.h"
#include "sd_manifest_request.h"
#include "sd_record_object_file.h"
#include "sd_recovery_loader.h"
#include "sd_repository_request.h"
#include "sd_storage_policy.h"
#include "sd_storage_work_gate.h"

namespace sticky_storage {

struct SdStorageServiceStatus {
    bool running = false;
    bool accepting_manifest_requests = false;
    bool accepting_journal_requests = false;
    bool accepting_repository_requests = false;
    bool accepting_work = false;
    uint32_t pending_work = 0U;
    SdCardState card_state = SdCardState::Absent;
    uint32_t mount_generation = 0U;
    bool capacity_known = false;
    uint64_t total_bytes = 0U;
    uint64_t free_bytes = 0U;
    bool manifest_known = false;
    SdManifestProbeState manifest_state =
        SdManifestProbeState::NotInitialized;
    bool manifest_selected = false;
    SdManifest selected_manifest = {};
    SdManifestSlot selected_slot = SdManifestSlot::A;
    bool recovery_known = false;
    SdRecoveryLoadStatus recovery_status =
        SdRecoveryLoadStatus::NotAttempted;
    bool recovery_view_ready = false;
    bool recovery_checkpoint_used = false;
    uint32_t recovery_journal_records = 0U;
    uint64_t recovery_last_transaction_id = 0U;
    uint64_t recovery_snapshot_transaction_id = 0U;
    uint32_t recovery_live_records = 0U;
    uint32_t recovery_verified_objects = 0U;
};

struct SdDataDeleteConfirmation {
    uint32_t mount_generation = 0U;
    bool manifest_selected = false;
    char dataset_id[kDatasetIdCharacters + 1U] = {};
    bool matches(uint32_t generation, bool selected, const char *dataset) const {
        return mount_generation != 0U && mount_generation == generation &&
               manifest_selected == selected &&
               (!selected || (dataset != nullptr &&
                 std::strncmp(dataset_id, dataset, sizeof(dataset_id)) == 0));
    }
};

struct SdStorageRequestHandle {
    uint32_t id = 0U;
    uint8_t slot = 0U;
};

struct SdStorageManifestResult {
    uint32_t request_id = 0U;
    // Generation captured by the submitting caller.
    uint32_t mount_generation = 0U;
    // Generation observed by the worker immediately before execution.
    uint32_t observed_mount_generation = 0U;
    SdManifestRequestResult outcome = {};
};

struct SdStorageJournalResult {
    uint32_t request_id = 0U;
    uint32_t mount_generation = 0U;
    uint32_t observed_mount_generation = 0U;
    SdJournalRequestResult outcome = {};
};

struct SdStorageRepositoryResult {
    uint32_t request_id = 0U;
    uint32_t mount_generation = 0U;
    uint32_t observed_mount_generation = 0U;
    SdRepositoryRequestResult outcome = {};
};

// Starts the single-owner MicroSD lifecycle task. A missing or unsupported
// card is a runtime state, not a firmware startup failure.
esp_err_t sd_storage_service_start();

// Returns a consistent card/admission snapshot. A caller must copy the mount
// generation into its request; the worker checks it again immediately before
// any filesystem operation.
SdStorageServiceStatus sd_storage_service_status();

// Non-blocking submission APIs. Initialization is rejected by the worker
// unless user_confirmed is true. ESP_OK means queued, not persisted; callers
// must keep the handle and collect the terminal result below.
esp_err_t sd_storage_service_submit_manifest_initialize(
    uint32_t mount_generation, const char *dataset_id, bool user_confirmed,
    SdStorageRequestHandle &handle);
esp_err_t sd_storage_service_submit_manifest_commit(
    uint32_t mount_generation, const SdManifest &manifest,
    SdStorageRequestHandle &handle);

// Returns ESP_ERR_NOT_FINISHED while the request is pending. A terminal result
// can be collected exactly once, which then releases its bounded result slot.
esp_err_t sd_storage_service_collect_manifest_result(
    const SdStorageRequestHandle &handle, SdStorageManifestResult &result);

// Journal operations use the same single-owner FIFO as manifest and sleep
// commands. Payload bytes are copied into the bounded request before return.
esp_err_t sd_storage_service_submit_journal_probe(
    uint32_t mount_generation, const char *dataset_id,
    SdStorageRequestHandle &handle);
esp_err_t sd_storage_service_submit_journal_append(
    uint32_t mount_generation, const char *dataset_id,
    SdJournalRecordType type, uint64_t transaction_id,
    const uint8_t *payload, size_t payload_size,
    SdStorageRequestHandle &handle);
esp_err_t sd_storage_service_collect_journal_result(
    const SdStorageRequestHandle &handle, SdStorageJournalResult &result);

// Production repository operations share the same owner FIFO as lifecycle,
// manifest and journal work. Requests and results are copied into a bounded
// service-owned pool; ESP_OK from submit means queued, never persisted.
esp_err_t sd_storage_service_submit_repository_read(
    uint32_t mount_generation, const char *dataset_id,
    const RepositoryKey &key, SdStorageRequestHandle &handle);
esp_err_t sd_storage_service_submit_repository_list(
    uint32_t mount_generation, const char *dataset_id,
    const RepositoryPageRequest &page, SdStorageRequestHandle &handle);
esp_err_t sd_storage_service_submit_repository_commit(
    uint32_t mount_generation, const char *dataset_id,
    const RepositoryMutation &mutation, uint64_t transaction_id,
    SdStorageRequestHandle &handle);
esp_err_t sd_storage_service_submit_repository_erase(
    uint32_t mount_generation, const char *dataset_id,
    const RepositoryKey &key, uint32_t expected_revision,
    uint64_t transaction_id, SdStorageRequestHandle &handle);
esp_err_t sd_storage_service_collect_repository_result(
    const SdStorageRequestHandle &handle,
    SdStorageRepositoryResult &result);

#if defined(STICKY_JOURNAL_POWER_TEST) && STICKY_JOURNAL_POWER_TEST
// Test-only request that prepares one immutable object through the storage
// owner before the physical journal power-loss loop is armed. It is absent
// from production builds and never appends a journal frame by itself.
esp_err_t sd_storage_service_submit_journal_power_fixture(
    uint32_t mount_generation, const char *dataset_id,
    SdStorageRequestHandle &handle);
#endif

#if (defined(STICKY_INDEX_POWER_TEST) && STICKY_INDEX_POWER_TEST) || \
    (defined(STICKY_CHECKPOINT_COMMIT_TEST) && \
     STICKY_CHECKPOINT_COMMIT_TEST)
struct SdStorageIndexPowerResult {
    uint32_t request_id = 0U;
    uint32_t mount_generation = 0U;
    uint32_t observed_mount_generation = 0U;
    esp_err_t service_status = ESP_ERR_INVALID_STATE;
    SdIndexWriteStatus write_status = SdIndexWriteStatus::InvalidArgument;
    bool changed = false;
    bool partial_prefix_match = false;
    SdIndexFileProbe probe = {};
};

// Isolated-test-only index candidate probe/write. The index power profile can
// write the candidate; the checkpoint profile only probes it before issuing a
// separate journal checkpoint request.
esp_err_t sd_storage_service_submit_index_power_probe(
    uint32_t mount_generation, const char *dataset_id,
    uint64_t last_transaction_id, SdStorageRequestHandle &handle);
esp_err_t sd_storage_service_submit_index_power_write(
    uint32_t mount_generation, const char *dataset_id,
    uint64_t last_transaction_id, SdStorageRequestHandle &handle);
esp_err_t sd_storage_service_collect_index_power_result(
    const SdStorageRequestHandle &handle,
    SdStorageIndexPowerResult &result);
#endif

#if defined(STICKY_OBJECT_POWER_TEST) && STICKY_OBJECT_POWER_TEST
struct SdStorageObjectPowerResult {
    uint32_t request_id = 0U;
    uint32_t mount_generation = 0U;
    uint32_t observed_mount_generation = 0U;
    esp_err_t service_status = ESP_ERR_INVALID_STATE;
    SdRecordObjectWriteStatus write_status =
        SdRecordObjectWriteStatus::InvalidArgument;
    bool changed = false;
    SdRecordObjectProbe probe = {};
};

// Isolated-test-only fixed object probe/write. The operation never appends a
// journal frame, so a complete or partial result remains uncommitted.
esp_err_t sd_storage_service_submit_object_power_probe(
    uint32_t mount_generation, const char *dataset_id,
    SdStorageRequestHandle &handle);
esp_err_t sd_storage_service_submit_object_power_write(
    uint32_t mount_generation, const char *dataset_id,
    SdStorageRequestHandle &handle);
esp_err_t sd_storage_service_collect_object_power_result(
    const SdStorageRequestHandle &handle,
    SdStorageObjectPowerResult &result);
#endif

// Record operations that span more than one worker request hold a bounded
// lease. A lease is bound to one mounted dataset, participates
// in the sleep drain, and must be finished or cancelled on every path. Finish
// reports a stale generation/dataset after a card switch and always releases a
// valid token; callers must not submit a late result after such a response.
SdStorageWorkStatus sd_storage_service_begin_work(
    SdStorageWorkKind kind, uint32_t mount_generation,
    const char *dataset_id, SdStorageWorkToken &token);
SdStorageWorkStatus sd_storage_service_finish_work(
    const SdStorageWorkToken &token, SdStorageWorkRecord &record);
bool sd_storage_service_cancel_work(const SdStorageWorkToken &token,
                                    SdStorageWorkRecord &record);

// Applies the current card state and capacity to a write of `requested_bytes`.
SdStoragePolicyStatus sd_storage_service_check_write(uint64_t requested_bytes);

// Stops admission and waits for the storage owner to finish ejecting the card.
// FIFO ordering drains every admitted filesystem request before unmount. Deep
// sleep must not continue when this handshake fails.
esp_err_t sd_storage_service_prepare_sleep(uint32_t timeout_ms = 5000U);

// Drains admitted work, unmounts the card and keeps it unmounted until the
// physical card is removed. This is the Settings-page safe-eject operation;
// unlike prepare_sleep(), it does not imply that the whole device will sleep.
esp_err_t sd_storage_service_safe_eject(uint32_t timeout_ms = 5000U);

// Deletes only the PREGJNL application tree after draining all accepted work.
// The caller must present an explicit confirmation; other card directories and
// device-local state are outside this operation.
esp_err_t sd_storage_service_delete_application_data(
    const SdDataDeleteConfirmation &confirmation, bool user_confirmed,
    uint32_t timeout_ms = 15000U);

}  // namespace sticky_storage
