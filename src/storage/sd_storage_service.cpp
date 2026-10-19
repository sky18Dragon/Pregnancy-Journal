#include "sd_storage_service.h"

#include <atomic>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include "esp_heap_caps.h"

#include <unistd.h>

#include "app_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sd_journal_completion.h"
#include "sd_index_power_fixture.h"
#include "sd_index_snapshot.h"
#include "sd_journal_power_fixture.h"
#include "sd_journal_request.h"
#include "sd_manifest_completion.h"
#include "sd_manifest_reader.h"
#include "sd_manifest_request.h"
#include "sd_object_power_fixture.h"
#include "sd_record_object_file.h"
#include "sd_recovery_loader.h"
#include "sd_repository_completion.h"
#include "sd_repository_request.h"
#include "sd_application_data_delete.h"
#include "sd_storage_layout.h"
#include "sd_storage_policy.h"
#include "sd_storage_work_gate.h"
#include "sticky_sdcard.h"
#include "storage_contract.h"
#include "storage_crc32.h"

namespace sticky_storage {
namespace {

constexpr char kTag[] = "sd_storage";
// Journal execution keeps a bounded 1,096-byte frame buffer plus request,
// probe and path state on this task's stack. Leave measured headroom for the
// FAT/VFS call chain instead of relying on the earlier manifest-only budget.
// Repository commits nest request/result buffers, journal verification and
// FatFs calls. The release call frames alone can exceed 8 KiB; keep room for
// driver/libc frames and verify the high-water mark on real mutations.
constexpr uint32_t kTaskStackSize = 16U * 1024U;
constexpr UBaseType_t kTaskPriority = 2U;
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(100);
constexpr uint32_t kRetryPollCount = 10U;
constexpr uint32_t kCapacityPollCount = 10U;

enum class CommandType : uint8_t {
    PrepareSleep = 0,
    DeleteApplicationData,
    Manifest,
    Journal,
    Repository,
#if defined(STICKY_JOURNAL_POWER_TEST) && STICKY_JOURNAL_POWER_TEST
    JournalPowerFixture,
#endif
#if defined(STICKY_OBJECT_POWER_TEST) && STICKY_OBJECT_POWER_TEST
    ObjectPowerProbe,
    ObjectPowerWrite,
#endif
#if (defined(STICKY_INDEX_POWER_TEST) && STICKY_INDEX_POWER_TEST) || \
    (defined(STICKY_CHECKPOINT_COMMIT_TEST) && \
     STICKY_CHECKPOINT_COMMIT_TEST)
    IndexPowerProbe,
    IndexPowerWrite,
#endif
};

struct Command {
    CommandType type = CommandType::PrepareSleep;
    uint32_t id = 0U;
    uint8_t completion_slot = 0U;
    SdDataDeleteConfirmation delete_confirmation = {};
    SdManifestRequest manifest = {};
    SdJournalRequest journal = {};
};

const SdManifestPaths kManifestPaths = {
    kApplicationRootPath,
    kManifestDirectoryPath,
    kManifestSlotAPath,
    kManifestSlotBPath,
};

const SdJournalStorePaths kJournalPaths = {
    kApplicationRootPath,
    kJournalDirectoryPath,
};

const SdRecoveryPaths kRecoveryPaths = {
    kApplicationRootPath,
    kJournalDirectoryPath,
    kIndexDirectoryPath,
    kRecordsDirectoryPath,
};

const SdRepositoryRequestPaths kRepositoryPaths = {
    kApplicationRootPath,
    kRecordsDirectoryPath,
    {kApplicationRootPath, kJournalDirectoryPath},
};

#if defined(STICKY_JOURNAL_POWER_TEST) && STICKY_JOURNAL_POWER_TEST
const SdJournalPowerFixturePaths kJournalPowerFixturePaths = {
    kApplicationRootPath,
    kRecordsDirectoryPath,
    "/sdcard/PREGJNL/RECORDS/PWRTEST.BIN",
};
#endif

#if (defined(STICKY_INDEX_POWER_TEST) && STICKY_INDEX_POWER_TEST) || \
    (defined(STICKY_CHECKPOINT_COMMIT_TEST) && \
     STICKY_CHECKPOINT_COMMIT_TEST)
const SdIndexFilePaths kIndexPowerPaths = {
    kApplicationRootPath,
    kIndexDirectoryPath,
#if defined(STICKY_INDEX_PARTIAL_TEST) && STICKY_INDEX_PARTIAL_TEST
    "/sdcard/PREGJNL/INDEX/IDXPWR02.BIN",
#else
    "/sdcard/PREGJNL/INDEX/IDXPWR01.BIN",
#endif
};

enum class IndexPowerCompletionState : uint8_t {
    Free = 0,
    Pending,
    Completed,
};

std::atomic<uint8_t> s_index_power_completion_state{
    static_cast<uint8_t>(IndexPowerCompletionState::Free)};
std::atomic<uint32_t> s_index_power_completion_id{0U};
uint32_t s_index_power_request_generation = 0U;
uint64_t s_index_power_last_transaction = 0U;
char s_index_power_dataset[kDatasetIdCharacters + 1U] = {};
SdStorageIndexPowerResult s_index_power_result = {};
std::array<uint8_t, kIndexPowerFixtureBytes> s_index_power_bytes = {};

#if defined(STICKY_INDEX_PARTIAL_TEST) && STICKY_INDEX_PARTIAL_TEST
bool index_power_partial_prefix_matches(const SdIndexFileProbe &probe)
{
    return probe.status == SdIndexFileProbeStatus::Mismatch &&
           probe.file_bytes == kIndexPowerPartialFixtureBytes &&
           probe.file_crc32 == crc32(s_index_power_bytes.data(),
                                     kIndexPowerPartialFixtureBytes);
}

SdIndexWriteResult write_index_power_partial_fixture(size_t encoded_size)
{
    SdIndexWriteResult result = {};
    result.probe = sd_index_file_probe(
        kIndexPowerPaths.index_file, s_index_power_bytes.data(), encoded_size);
    if (result.probe.status == SdIndexFileProbeStatus::Match ||
        result.probe.status == SdIndexFileProbeStatus::Mismatch) {
        result.status = SdIndexWriteStatus::ExistingConflict;
        return result;
    }
    if (result.probe.status != SdIndexFileProbeStatus::Missing) {
        result.status = result.probe.status ==
                                SdIndexFileProbeStatus::InvalidArgument
                            ? SdIndexWriteStatus::InvalidArgument
                            : SdIndexWriteStatus::ProbeIoError;
        return result;
    }

    std::FILE *file = std::fopen(kIndexPowerPaths.index_file, "wb");
    if (file == nullptr) {
        result.status = SdIndexWriteStatus::OpenError;
        return result;
    }
    SdIndexWriteStatus write_status = SdIndexWriteStatus::ShortWrite;
    if (std::fwrite(s_index_power_bytes.data(), 1U,
                    kIndexPowerPartialFixtureBytes, file) !=
        kIndexPowerPartialFixtureBytes) {
        write_status = SdIndexWriteStatus::ShortWrite;
    } else if (std::fflush(file) != 0 || ::fsync(fileno(file)) != 0) {
        write_status = SdIndexWriteStatus::SyncError;
    }
    if (std::fclose(file) != 0 &&
        write_status == SdIndexWriteStatus::ShortWrite) {
        write_status = SdIndexWriteStatus::CloseError;
    }
    result.status = write_status;
    result.probe = sd_index_file_probe(
        kIndexPowerPaths.index_file, s_index_power_bytes.data(), encoded_size);
    result.changed = result.status == SdIndexWriteStatus::ShortWrite &&
                     index_power_partial_prefix_matches(result.probe);
    if (!result.changed && result.status == SdIndexWriteStatus::ShortWrite) {
        result.status = SdIndexWriteStatus::VerifyError;
    }
    return result;
}
#endif
#endif

#if (defined(STICKY_INDEX_POWER_TEST) && STICKY_INDEX_POWER_TEST) || \
    (defined(STICKY_CHECKPOINT_COMMIT_TEST) && \
     STICKY_CHECKPOINT_COMMIT_TEST)
void complete_index_power_request(
    uint32_t id, uint32_t request_generation,
    uint32_t observed_generation, const SdStorageIndexPowerResult &outcome)
{
    if (s_index_power_completion_id.load(std::memory_order_acquire) != id ||
        static_cast<IndexPowerCompletionState>(
            s_index_power_completion_state.load(std::memory_order_acquire)) !=
            IndexPowerCompletionState::Pending) {
        STICKY_LOGE(kTag, "index_power=request id=%u completion=invalid",
                    static_cast<unsigned>(id));
        return;
    }
    s_index_power_result = outcome;
    s_index_power_result.request_id = id;
    s_index_power_result.mount_generation = request_generation;
    s_index_power_result.observed_mount_generation = observed_generation;
    s_index_power_completion_state.store(
        static_cast<uint8_t>(IndexPowerCompletionState::Completed),
        std::memory_order_release);
}

void process_index_power_request(StickySdCard &card,
                                 const Command &command,
                                 bool write)
{
    SdStorageIndexPowerResult result = {};
    result.service_status = ESP_ERR_INVALID_STATE;
    const uint32_t observed_generation = card.generation();
    const SdManifestProbeResult manifest =
        card.accepts(s_index_power_request_generation, write)
            ? sd_manifest_probe(kManifestSlotAPath, kManifestSlotBPath)
            : SdManifestProbeResult{};
    const bool selected = manifest.state == SdManifestProbeState::Ready ||
                          manifest.state == SdManifestProbeState::Degraded;
    const size_t encoded_size = selected
        ? sd_index_power_fixture_bytes(
              s_index_power_dataset, s_index_power_last_transaction,
              s_index_power_bytes)
        : 0U;

    if (!card.accepts(s_index_power_request_generation, write)) {
        result.service_status = card.state() == SdCardState::ReadyReadOnly &&
                                        write
                                    ? ESP_ERR_NOT_ALLOWED
                                    : ESP_ERR_INVALID_STATE;
    } else if (!selected || encoded_size == 0U) {
        result.service_status = ESP_ERR_INVALID_STATE;
    } else if (std::strcmp(s_index_power_dataset,
                           manifest.selected.dataset_id) != 0) {
        result.service_status = ESP_ERR_INVALID_ARG;
    } else {
        const SdJournalStoreProbe journal = sd_journal_store_probe(
            kJournalPaths, s_index_power_dataset);
        bool journal_matches =
            journal.status == SdJournalStoreStatus::Ready &&
            journal.total_record_count != 0U &&
            journal.active.last.transaction_id ==
                s_index_power_last_transaction;
#if defined(STICKY_CHECKPOINT_COMMIT_TEST) && \
    STICKY_CHECKPOINT_COMMIT_TEST
        journal_matches = journal_matches ||
            (!write && journal.status == SdJournalStoreStatus::Ready &&
             journal.active.last.type == SdJournalRecordType::Checkpoint &&
             journal.active.last.transaction_id ==
                 s_index_power_last_transaction + 1U);
#endif
        if (!journal_matches) {
            result.service_status = ESP_ERR_INVALID_STATE;
        } else {
            result.service_status = ESP_OK;
        }
    }
    if (result.service_status == ESP_OK && write) {
        StickySdCardSpace space = {};
        if (card.space(space) != ESP_OK) {
            result.service_status = ESP_FAIL;
        } else {
            const SdStorageUsage usage = {space.free_bytes};
            if (sd_storage_check_write(encoded_size, usage) !=
                SdStoragePolicyStatus::Ok) {
                result.service_status = ESP_ERR_NO_MEM;
            }
        }
    }
    if (result.service_status == ESP_OK) {
        if (write) {
#if defined(STICKY_INDEX_PARTIAL_TEST) && STICKY_INDEX_PARTIAL_TEST
            const SdIndexWriteResult written =
                write_index_power_partial_fixture(encoded_size);
#else
            const SdIndexWriteResult written = sd_index_file_write(
                kIndexPowerPaths, s_index_power_bytes.data(), encoded_size);
#endif
            result.write_status = written.status;
            result.changed = written.changed;
            result.probe = written.probe;
        } else {
            result.write_status = SdIndexWriteStatus::Ok;
            result.probe = sd_index_file_probe(
                kIndexPowerPaths.index_file, s_index_power_bytes.data(),
                encoded_size);
        }
#if defined(STICKY_INDEX_PARTIAL_TEST) && STICKY_INDEX_PARTIAL_TEST
        result.partial_prefix_match =
            index_power_partial_prefix_matches(result.probe);
#endif
    }
    complete_index_power_request(
        command.id, s_index_power_request_generation,
        observed_generation, result);
    STICKY_LOGI(
        kTag,
        "index_power=request id=%u generation=%u observed=%u write=%u last_tx=%llu service=%s status=%u probe=%u bytes=%llu changed=%u",
        static_cast<unsigned>(command.id),
        static_cast<unsigned>(s_index_power_request_generation),
        static_cast<unsigned>(observed_generation), write ? 1U : 0U,
        static_cast<unsigned long long>(s_index_power_last_transaction),
        esp_err_to_name(result.service_status),
        static_cast<unsigned>(result.write_status),
        static_cast<unsigned>(result.probe.status),
        static_cast<unsigned long long>(result.probe.file_bytes),
        result.changed ? 1U : 0U);
}
#endif

#if defined(STICKY_OBJECT_POWER_TEST) && STICKY_OBJECT_POWER_TEST
const SdRecordObjectPaths kObjectPowerPaths = {
    kApplicationRootPath,
    kRecordsDirectoryPath,
#if defined(STICKY_OBJECT_PARTIAL_TEST) && STICKY_OBJECT_PARTIAL_TEST
    "/sdcard/PREGJNL/RECORDS/OBJPWR02.BIN",
#else
    "/sdcard/PREGJNL/RECORDS/OBJPWR01.BIN",
#endif
};

enum class ObjectPowerCompletionState : uint8_t {
    Free = 0,
    Pending,
    Completed,
};

std::atomic<uint8_t> s_object_power_completion_state{
    static_cast<uint8_t>(ObjectPowerCompletionState::Free)};
std::atomic<uint32_t> s_object_power_completion_id{0U};
uint32_t s_object_power_request_generation = 0U;
char s_object_power_dataset[kDatasetIdCharacters + 1U] = {};
SdStorageObjectPowerResult s_object_power_result = {};

#if defined(STICKY_OBJECT_PARTIAL_TEST) && STICKY_OBJECT_PARTIAL_TEST
SdRecordObjectWriteResult write_object_power_partial_fixture(
    const uint8_t *bytes, size_t size)
{
    SdRecordObjectWriteResult result = {};
    result.probe = sd_record_object_probe(
        kObjectPowerPaths.object, bytes, size);
    if (result.probe.status == SdRecordObjectProbeStatus::Match ||
        result.probe.status == SdRecordObjectProbeStatus::Mismatch) {
        result.status = SdRecordObjectWriteStatus::ExistingConflict;
        return result;
    }
    if (result.probe.status != SdRecordObjectProbeStatus::Missing) {
        result.status = result.probe.status ==
                                SdRecordObjectProbeStatus::InvalidArgument
                            ? SdRecordObjectWriteStatus::InvalidArgument
                            : SdRecordObjectWriteStatus::ProbeIoError;
        return result;
    }

    std::FILE *file = std::fopen(kObjectPowerPaths.object, "wb");
    if (file == nullptr) {
        result.status = SdRecordObjectWriteStatus::OpenError;
        return result;
    }
    SdRecordObjectWriteStatus write_status =
        SdRecordObjectWriteStatus::ShortWrite;
    if (std::fwrite(bytes, 1U, kObjectPowerPartialFixtureBytes, file) !=
        kObjectPowerPartialFixtureBytes) {
        write_status = SdRecordObjectWriteStatus::ShortWrite;
    } else if (std::fflush(file) != 0 || ::fsync(fileno(file)) != 0) {
        write_status = SdRecordObjectWriteStatus::SyncError;
    }
    if (std::fclose(file) != 0 &&
        write_status == SdRecordObjectWriteStatus::ShortWrite) {
        write_status = SdRecordObjectWriteStatus::CloseError;
    }
    result.status = write_status;
    result.probe = sd_record_object_probe(
        kObjectPowerPaths.object, bytes, size);
    result.changed = result.status == SdRecordObjectWriteStatus::ShortWrite &&
                     result.probe.status ==
                         SdRecordObjectProbeStatus::Mismatch &&
                     result.probe.file_bytes ==
                         kObjectPowerPartialFixtureBytes &&
                     result.probe.file_crc32 ==
                         crc32(bytes, kObjectPowerPartialFixtureBytes);
    if (!result.changed &&
        result.status == SdRecordObjectWriteStatus::ShortWrite) {
        result.status = SdRecordObjectWriteStatus::VerifyError;
    }
    return result;
}
#endif
#endif

QueueHandle_t s_commands = nullptr;
SemaphoreHandle_t s_submit_lock = nullptr;
TaskHandle_t s_task = nullptr;
std::atomic<uint32_t> s_next_command_id{0U};
std::atomic<uint32_t> s_completed_command_id{0U};
std::atomic<int> s_completed_result{ESP_OK};
std::atomic<bool> s_control_busy{false};
std::atomic<bool> s_sleep_requested{false};
std::atomic<bool> s_manual_eject_hold{false};
std::atomic<bool> s_running{false};
std::atomic<bool> s_accepting_manifest_requests{false};
std::atomic<bool> s_accepting_journal_requests{false};
std::atomic<bool> s_accepting_repository_requests{false};
std::atomic<bool> s_accepting_work{false};
std::atomic<uint32_t> s_pending_file_requests{0U};
std::atomic<uint32_t> s_pending_work{0U};
std::atomic<uint32_t> s_status_revision{0U};
std::atomic<uint8_t> s_card_state{
    static_cast<uint8_t>(SdCardState::Absent)};
std::atomic<uint32_t> s_card_generation{0U};
std::atomic<bool> s_capacity_known{false};
std::atomic<uint64_t> s_total_bytes{0U};
std::atomic<uint64_t> s_free_bytes{0U};
std::atomic<bool> s_manifest_known{false};
std::atomic<uint8_t> s_manifest_state{
    static_cast<uint8_t>(SdManifestProbeState::NotInitialized)};
portMUX_TYPE s_manifest_snapshot_lock = portMUX_INITIALIZER_UNLOCKED;
bool s_manifest_selected = false;
SdManifest s_selected_manifest = {};
SdManifestSlot s_selected_slot = SdManifestSlot::A;
std::atomic<bool> s_recovery_known{false};
std::atomic<uint8_t> s_recovery_status{
    static_cast<uint8_t>(SdRecoveryLoadStatus::NotAttempted)};
std::atomic<bool> s_recovery_view_ready{false};
std::atomic<bool> s_recovery_checkpoint_used{false};
std::atomic<uint32_t> s_recovery_journal_records{0U};
std::atomic<uint64_t> s_recovery_last_transaction_id{0U};
std::atomic<uint64_t> s_recovery_snapshot_transaction_id{0U};
std::atomic<uint32_t> s_recovery_live_records{0U};
std::atomic<uint32_t> s_recovery_verified_objects{0U};
// Keep the expanded recovery buffers off both task stacks and internal SRAM.
// Allocation is mandatory: without PSRAM the service refuses to start.
struct RecoveryState {
    SdRepositoryRecoveryIndex index;
    SdRecoveryWorkspace workspace;
};
RecoveryState *s_recovery = nullptr;
SdManifestCompletionPool s_manifest_completions;
SdJournalCompletionPool s_journal_completions;
SdRepositoryCompletionPool s_repository_completions;
SdRepositoryRequestResult s_repository_result_workspace;
SdStorageWorkGate s_work_gate;

void open_admission_locked()
{
    s_work_gate.open_admission();
    s_accepting_manifest_requests.store(true, std::memory_order_release);
    s_accepting_journal_requests.store(true, std::memory_order_release);
    s_accepting_repository_requests.store(true, std::memory_order_release);
    s_accepting_work.store(true, std::memory_order_release);
}

void close_admission_locked()
{
    s_work_gate.close_admission();
    s_accepting_manifest_requests.store(false, std::memory_order_release);
    s_accepting_journal_requests.store(false, std::memory_order_release);
    s_accepting_repository_requests.store(false, std::memory_order_release);
    s_accepting_work.store(false, std::memory_order_release);
}

bool selected_dataset_snapshot(uint32_t &mount_generation,
                               char *dataset_id)
{
    mount_generation = s_card_generation.load(std::memory_order_acquire);
    bool selected = false;
    portENTER_CRITICAL(&s_manifest_snapshot_lock);
    selected = s_manifest_selected;
    if (selected) {
        std::memcpy(dataset_id, s_selected_manifest.dataset_id,
                    sizeof(s_selected_manifest.dataset_id));
    }
    portEXIT_CRITICAL(&s_manifest_snapshot_lock);
    return selected;
}

const char *state_name(SdCardState state)
{
    switch (state) {
    case SdCardState::Absent:
        return "absent";
    case SdCardState::Mounting:
        return "mounting";
    case SdCardState::ReadyReadWrite:
        return "ready_read_write";
    case SdCardState::ReadyReadOnly:
        return "ready_read_only";
    case SdCardState::Full:
        return "full";
    case SdCardState::Fault:
        return "fault";
    case SdCardState::Ejecting:
        return "ejecting";
    case SdCardState::Unmounted:
        return "unmounted";
    }
    return "unknown";
}

void publish_card_status(const StickySdCard &card)
{
    s_status_revision.fetch_add(1U, std::memory_order_acq_rel);
    s_card_generation.store(card.generation(), std::memory_order_relaxed);
    s_card_state.store(static_cast<uint8_t>(card.state()),
                       std::memory_order_relaxed);
    if (!card.mounted()) {
        s_capacity_known.store(false, std::memory_order_relaxed);
        s_total_bytes.store(0U, std::memory_order_relaxed);
        s_free_bytes.store(0U, std::memory_order_relaxed);
        s_manifest_known.store(false, std::memory_order_relaxed);
        s_manifest_state.store(
            static_cast<uint8_t>(SdManifestProbeState::NotInitialized),
            std::memory_order_relaxed);
        s_recovery_known.store(false, std::memory_order_relaxed);
        s_recovery_status.store(
            static_cast<uint8_t>(SdRecoveryLoadStatus::NotAttempted),
            std::memory_order_relaxed);
        s_recovery_view_ready.store(false, std::memory_order_relaxed);
        s_recovery_checkpoint_used.store(false, std::memory_order_relaxed);
        s_recovery_journal_records.store(0U, std::memory_order_relaxed);
        s_recovery_last_transaction_id.store(0U,
                                              std::memory_order_relaxed);
        s_recovery_snapshot_transaction_id.store(
            0U, std::memory_order_relaxed);
        s_recovery_live_records.store(0U, std::memory_order_relaxed);
        s_recovery_verified_objects.store(0U, std::memory_order_relaxed);
        s_recovery->index.reset();
        portENTER_CRITICAL(&s_manifest_snapshot_lock);
        s_manifest_selected = false;
        s_selected_manifest = {};
        s_selected_slot = SdManifestSlot::A;
        portEXIT_CRITICAL(&s_manifest_snapshot_lock);
    }
    s_status_revision.fetch_add(1U, std::memory_order_release);
}

void publish_recovery_status(const SdRecoveryLoadResult &result)
{
    s_status_revision.fetch_add(1U, std::memory_order_acq_rel);
    s_recovery_status.store(static_cast<uint8_t>(result.status),
                            std::memory_order_relaxed);
    s_recovery_view_ready.store(result.view_ready,
                                std::memory_order_relaxed);
    s_recovery_checkpoint_used.store(result.checkpoint_used,
                                     std::memory_order_relaxed);
    s_recovery_journal_records.store(result.journal_records,
                                     std::memory_order_relaxed);
    s_recovery_last_transaction_id.store(result.last_transaction_id,
                                         std::memory_order_relaxed);
    s_recovery_snapshot_transaction_id.store(
        result.snapshot_transaction_id, std::memory_order_relaxed);
    s_recovery_live_records.store(result.live_records,
                                  std::memory_order_relaxed);
    s_recovery_verified_objects.store(result.verified_objects,
                                      std::memory_order_relaxed);
    s_recovery_known.store(true, std::memory_order_relaxed);
    s_status_revision.fetch_add(1U, std::memory_order_release);
}

SdRecoveryLoadResult refresh_recovery(const SdManifestProbeResult &manifest)
{
    SdRecoveryLoadResult result = {};
    const bool selected = manifest.state == SdManifestProbeState::Ready ||
                          manifest.state == SdManifestProbeState::Degraded;
    if (selected) {
        result = sd_recovery_load(kRecoveryPaths,
                                  manifest.selected.dataset_id,
                                  s_recovery->index,
                                  s_recovery->workspace);
    } else {
        s_recovery->index.reset();
    }
    publish_recovery_status(result);
    STICKY_LOGI(kTag, "recovery=memory stack_free_bytes=%u internal_free=%u psram_free=%u",
                static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
    STICKY_LOGI(
        kTag,
        "recovery=status=%u ready=%u checkpoint=%u journal=%u last_tx=%llu snapshot_tx=%llu live=%u objects=%u",
        static_cast<unsigned>(result.status), result.view_ready ? 1U : 0U,
        result.checkpoint_used ? 1U : 0U,
        static_cast<unsigned>(result.journal_records),
        static_cast<unsigned long long>(result.last_transaction_id),
        static_cast<unsigned long long>(result.snapshot_transaction_id),
        static_cast<unsigned>(result.live_records),
        static_cast<unsigned>(result.verified_objects));
    return result;
}

void publish_capacity_status(bool known, uint64_t total_bytes,
                             uint64_t free_bytes)
{
    s_status_revision.fetch_add(1U, std::memory_order_acq_rel);
    s_total_bytes.store(known ? total_bytes : 0U,
                        std::memory_order_relaxed);
    s_free_bytes.store(known ? free_bytes : 0U,
                       std::memory_order_relaxed);
    s_capacity_known.store(known, std::memory_order_relaxed);
    s_status_revision.fetch_add(1U, std::memory_order_release);
}

esp_err_t refresh_capacity(StickySdCard &card)
{
    StickySdCardSpace space = {};
    const esp_err_t result = card.space(space);
    if (result != ESP_OK) {
        publish_capacity_status(false, 0U, 0U);
        return result;
    }
    const bool was_known = s_capacity_known.load(std::memory_order_acquire);
    const uint64_t previous_total =
        s_total_bytes.load(std::memory_order_relaxed);
    const uint64_t previous_free =
        s_free_bytes.load(std::memory_order_relaxed);
    if (space.free_bytes == 0U &&
        card.state() == SdCardState::ReadyReadWrite) {
        card.mark_full();
    } else if (space.free_bytes != 0U &&
               card.state() == SdCardState::Full) {
        card.mark_space_available();
    }
    publish_capacity_status(true, space.total_bytes, space.free_bytes);
    publish_card_status(card);
    if (!was_known || previous_total != space.total_bytes ||
        previous_free != space.free_bytes) {
        STICKY_LOGI(kTag, "card=capacity total=%llu free=%llu",
                    static_cast<unsigned long long>(space.total_bytes),
                    static_cast<unsigned long long>(space.free_bytes));
    }
    return ESP_OK;
}

void publish_manifest_status(const SdManifestProbeResult &probe)
{
    s_status_revision.fetch_add(1U, std::memory_order_acq_rel);
    s_manifest_state.store(static_cast<uint8_t>(probe.state),
                           std::memory_order_relaxed);
    portENTER_CRITICAL(&s_manifest_snapshot_lock);
    s_manifest_selected = probe.state == SdManifestProbeState::Ready ||
                          probe.state == SdManifestProbeState::Degraded;
    s_selected_manifest = s_manifest_selected ? probe.selected : SdManifest{};
    s_selected_slot = s_manifest_selected ? probe.selected_slot
                                          : SdManifestSlot::A;
    portEXIT_CRITICAL(&s_manifest_snapshot_lock);
    s_manifest_known.store(true, std::memory_order_relaxed);
    s_status_revision.fetch_add(1U, std::memory_order_release);
}

void complete_manifest_request(uint8_t slot, uint32_t id,
                               uint32_t observed_mount_generation,
                               const SdManifestRequestResult &result)
{
    const SdManifestCompletionToken token = {id, slot};
    if (!s_manifest_completions.complete(token, observed_mount_generation,
                                         result)) {
        STICKY_LOGE(kTag, "manifest=request id=%u completion=invalid",
                    static_cast<unsigned>(id));
    }
}

void process_manifest_request(StickySdCard &card, const Command &command)
{
    const SdManifestRequestContext context = {
        card.state(),
        card.generation(),
    };
    const SdManifestRequestResult result = sd_manifest_execute_request(
        command.manifest, context, kManifestPaths);
    if (result.status == SdManifestRequestStatus::Ok) {
        const SdManifestProbeResult manifest =
            sd_manifest_probe(kManifestSlotAPath, kManifestSlotBPath);
        // Initialization must make the new empty repository usable without a
        // remount. Refresh its view before exposing the selected manifest or
        // completing the request; deletion cleared the previous recovery view.
        refresh_recovery(manifest);
        publish_manifest_status(manifest);
    }
    complete_manifest_request(command.completion_slot, command.id,
                              context.mount_generation, result);
    STICKY_LOGI(kTag,
                "manifest=request id=%u requested_generation=%u observed_generation=%u result=%u changed=%u",
                static_cast<unsigned>(command.id),
                static_cast<unsigned>(command.manifest.mount_generation),
                static_cast<unsigned>(context.mount_generation),
                static_cast<unsigned>(result.status), result.changed ? 1U : 0U);
}

void complete_journal_request(uint8_t slot, uint32_t id,
                              uint32_t observed_mount_generation,
                              const SdJournalRequestResult &result)
{
    const SdJournalCompletionToken token = {id, slot};
    if (!s_journal_completions.complete(token, observed_mount_generation,
                                        result)) {
        STICKY_LOGE(kTag, "journal=request id=%u completion=invalid",
                    static_cast<unsigned>(id));
    }
}

void process_journal_request(StickySdCard &card, const Command &command)
{
    SdManifestProbeResult manifest = {};
    if (card.accepts(command.journal.mount_generation, false)) {
        manifest = sd_manifest_probe(kManifestSlotAPath, kManifestSlotBPath);
    }
    const bool selected = manifest.state == SdManifestProbeState::Ready ||
                          manifest.state == SdManifestProbeState::Degraded;
    const SdJournalRequestContext context = {
        card.state(),
        card.generation(),
        manifest.state,
        selected,
        selected ? manifest.selected : SdManifest{},
    };
    SdJournalRequestResult result = {};
    bool execute = true;
    if (command.journal.operation == SdJournalRequestOperation::Append &&
        card.accepts(command.journal.mount_generation, true) && selected &&
        std::strcmp(command.journal.dataset_id,
                    manifest.selected.dataset_id) == 0) {
        StickySdCardSpace space = {};
        if (card.space(space) != ESP_OK) {
            result.status = SdJournalRequestStatus::CapacityUnavailable;
            execute = false;
        } else {
            const SdStorageUsage usage = {space.free_bytes};
            const uint64_t frame_bytes =
                static_cast<uint64_t>(kSdJournalHeaderBytes) +
                command.journal.payload_size;
            if (sd_storage_check_write(frame_bytes, usage) !=
                SdStoragePolicyStatus::Ok) {
                result.status = SdJournalRequestStatus::NoSpace;
                execute = false;
            }
        }
    }
    if (execute) {
        result = sd_journal_execute_request(command.journal, context,
                                            kJournalPaths);
    }
    complete_journal_request(command.completion_slot, command.id,
                             context.mount_generation, result);
    STICKY_LOGI(
        kTag,
        "journal=request id=%u requested_generation=%u observed_generation=%u operation=%u result=%u changed=%u",
        static_cast<unsigned>(command.id),
        static_cast<unsigned>(command.journal.mount_generation),
        static_cast<unsigned>(context.mount_generation),
        static_cast<unsigned>(command.journal.operation),
        static_cast<unsigned>(result.status), result.changed ? 1U : 0U);
}

void process_repository_request(StickySdCard &card, const Command &command)
{
    const SdRepositoryCompletionToken token = {
        command.id, command.completion_slot};
    const SdRepositoryRequest *request = nullptr;
    if (!s_repository_completions.pending_request(token, request) ||
        request == nullptr) {
        STICKY_LOGE(kTag, "repository=request id=%u pending=invalid",
                    static_cast<unsigned>(command.id));
        return;
    }

    SdManifestProbeResult manifest = {};
    if (card.accepts(request->mount_generation, false)) {
        manifest = sd_manifest_probe(kManifestSlotAPath, kManifestSlotBPath);
    }
    const bool selected = manifest.state == SdManifestProbeState::Ready ||
                          manifest.state == SdManifestProbeState::Degraded;
    StickySdCardSpace space = {};
    const bool capacity_known = card.can_read() && card.space(space) == ESP_OK;
    const SdRepositoryRequestContext context = {
        card.state(),
        card.generation(),
        capacity_known,
        capacity_known ? space.free_bytes : 0U,
        manifest.state,
        selected,
        selected ? manifest.selected : SdManifest{},
        s_recovery_view_ready.load(std::memory_order_acquire),
    };
    s_repository_result_workspace = sd_repository_execute_request(
        *request, context, kRepositoryPaths, s_recovery->index);
    if (s_repository_result_workspace.status ==
            SdRepositoryRequestStatus::Ok &&
        s_repository_result_workspace.changed) {
        const SdRecoveryLoadResult recovered = refresh_recovery(manifest);
        sd_repository_validate_recovered_commit(
            s_repository_result_workspace, recovered.view_ready,
            recovered.last_transaction_id);
    }
    const uint32_t requested_generation = request->mount_generation;
    const SdRepositoryRequestOperation operation = request->operation;
    const uint64_t transaction_id =
        s_repository_result_workspace.batch.transaction_id != 0U
            ? s_repository_result_workspace.batch.transaction_id
            : request->transaction_id;
    if (!s_repository_completions.complete(
            token, context.mount_generation,
            s_repository_result_workspace)) {
        STICKY_LOGE(kTag, "repository=request id=%u completion=invalid",
                    static_cast<unsigned>(command.id));
    }
    STICKY_LOGI(
        kTag,
        "repository=request id=%u requested_generation=%u observed_generation=%u operation=%u tx=%llu result=%u changed=%u stack_free_bytes=%u",
        static_cast<unsigned>(command.id),
        static_cast<unsigned>(requested_generation),
        static_cast<unsigned>(context.mount_generation),
        static_cast<unsigned>(operation),
        static_cast<unsigned long long>(transaction_id),
        static_cast<unsigned>(s_repository_result_workspace.status),
        s_repository_result_workspace.changed ? 1U : 0U,
        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
}

#if defined(STICKY_JOURNAL_POWER_TEST) && STICKY_JOURNAL_POWER_TEST
SdJournalRequestStatus map_power_fixture_status(
    SdJournalPowerFixtureStatus status)
{
    switch (status) {
    case SdJournalPowerFixtureStatus::Ok:
        return SdJournalRequestStatus::Ok;
    case SdJournalPowerFixtureStatus::InvalidArgument:
        return SdJournalRequestStatus::InvalidArgument;
    case SdJournalPowerFixtureStatus::DirectoryError:
        return SdJournalRequestStatus::DirectoryError;
    case SdJournalPowerFixtureStatus::OpenError:
        return SdJournalRequestStatus::OpenError;
    case SdJournalPowerFixtureStatus::ShortWrite:
        return SdJournalRequestStatus::ShortWrite;
    case SdJournalPowerFixtureStatus::SyncError:
        return SdJournalRequestStatus::SyncError;
    case SdJournalPowerFixtureStatus::CloseError:
        return SdJournalRequestStatus::CloseError;
    case SdJournalPowerFixtureStatus::VerifyError:
        return SdJournalRequestStatus::VerifyError;
    }
    return SdJournalRequestStatus::VerifyError;
}

#if defined(STICKY_JOURNAL_PARTIAL_TEST) && STICKY_JOURNAL_PARTIAL_TEST
bool journal_partial_baseline_matches(const SdJournalStoreProbe &probe)
{
    return probe.status == SdJournalStoreStatus::Ready &&
           probe.segment_count == 1U && probe.active_segment == 1U &&
           probe.total_record_count == kJournalPartialBaselineRecords &&
           probe.total_bytes == kJournalPartialBaselineBytes &&
           probe.active.file_bytes == kJournalPartialBaselineBytes &&
           probe.active.valid_bytes == kJournalPartialBaselineBytes &&
           probe.active.last.transaction_id ==
               kJournalPartialBaselineTransaction;
}

bool journal_partial_result_matches(const SdJournalStoreProbe &probe)
{
    return probe.status == SdJournalStoreStatus::TailTruncated &&
           probe.segment_count == 1U && probe.active_segment == 1U &&
           probe.total_record_count == kJournalPartialBaselineRecords &&
           probe.active.file_bytes == kJournalPartialExpectedFileBytes &&
           probe.active.valid_bytes == kJournalPartialBaselineBytes &&
           probe.active.last.transaction_id ==
               kJournalPartialBaselineTransaction;
}

SdJournalPowerFixtureStatus append_journal_partial_fixture(
    const char *dataset_id)
{
    std::array<uint8_t, kSdJournalMaxRecordBytes> frame = {};
    const size_t frame_size = sd_journal_power_fixture_frame(
        dataset_id, kJournalPartialTargetTransaction, frame);
    if (frame_size != kJournalPartialFixtureFrameBytes) {
        return SdJournalPowerFixtureStatus::InvalidArgument;
    }
    std::FILE *file = std::fopen(kJournalFirstSegmentPath, "ab");
    if (file == nullptr) {
        return SdJournalPowerFixtureStatus::OpenError;
    }
    SdJournalPowerFixtureStatus status =
        SdJournalPowerFixtureStatus::Ok;
    if (std::fwrite(frame.data(), 1U, kJournalPartialFixtureTailBytes,
                    file) != kJournalPartialFixtureTailBytes) {
        status = SdJournalPowerFixtureStatus::ShortWrite;
    } else if (std::fflush(file) != 0 || ::fsync(fileno(file)) != 0) {
        status = SdJournalPowerFixtureStatus::SyncError;
    }
    if (std::fclose(file) != 0 &&
        status == SdJournalPowerFixtureStatus::Ok) {
        status = SdJournalPowerFixtureStatus::CloseError;
    }
    return status;
}
#endif

void process_journal_power_fixture(StickySdCard &card,
                                   const Command &command)
{
    SdJournalRequestResult result = {};
    const uint32_t observed_generation = card.generation();
    const SdManifestProbeResult manifest =
        card.accepts(command.journal.mount_generation, true)
            ? sd_manifest_probe(kManifestSlotAPath, kManifestSlotBPath)
            : SdManifestProbeResult{};
    const bool selected = manifest.state == SdManifestProbeState::Ready ||
                          manifest.state == SdManifestProbeState::Degraded;
    if (!card.accepts(command.journal.mount_generation, true)) {
        result.status = card.state() == SdCardState::ReadyReadOnly
                            ? SdJournalRequestStatus::CardReadOnly
                            : SdJournalRequestStatus::CardUnavailable;
    } else if (!selected) {
        result.status = SdJournalRequestStatus::ManifestUnavailable;
    } else if (std::strcmp(command.journal.dataset_id,
                           manifest.selected.dataset_id) != 0) {
        result.status = SdJournalRequestStatus::DatasetMismatch;
    } else {
#if defined(STICKY_JOURNAL_PARTIAL_TEST) && STICKY_JOURNAL_PARTIAL_TEST
        const SdJournalStoreProbe before = sd_journal_store_probe(
            kJournalPaths, command.journal.dataset_id);
        if (!journal_partial_baseline_matches(before)) {
            result.status = SdJournalRequestStatus::VerifyError;
        } else {
            StickySdCardSpace space = {};
            if (card.space(space) != ESP_OK) {
                result.status = SdJournalRequestStatus::CapacityUnavailable;
            } else {
                const SdStorageUsage usage = {space.free_bytes};
                if (sd_storage_check_write(
                        kJournalPartialFixtureTailBytes, usage) !=
                    SdStoragePolicyStatus::Ok) {
                    result.status = SdJournalRequestStatus::NoSpace;
                } else {
                    const SdJournalPowerFixtureStatus fixture =
                        append_journal_partial_fixture(
                            command.journal.dataset_id);
                    result.status = map_power_fixture_status(fixture);
                    if (fixture == SdJournalPowerFixtureStatus::Ok) {
                        result.probe = sd_journal_store_probe(
                            kJournalPaths, command.journal.dataset_id);
                        if (journal_partial_result_matches(result.probe)) {
                            result.status =
                                SdJournalRequestStatus::TailTruncated;
                            result.changed = true;
                        } else {
                            result.status =
                                SdJournalRequestStatus::VerifyError;
                        }
                    }
                }
            }
        }
#else
        StickySdCardSpace space = {};
        if (card.space(space) != ESP_OK) {
            result.status = SdJournalRequestStatus::CapacityUnavailable;
        } else {
            const SdStorageUsage usage = {space.free_bytes};
            if (sd_storage_check_write(
                    kJournalPowerFixtureObjectBytes, usage) !=
                SdStoragePolicyStatus::Ok) {
                result.status = SdJournalRequestStatus::NoSpace;
            } else {
                const SdJournalPowerFixtureResult fixture =
                    sd_journal_power_fixture_prepare(
                        kJournalPowerFixturePaths);
                result.status = map_power_fixture_status(fixture.status);
                result.changed = fixture.changed;
                if (result.status == SdJournalRequestStatus::Ok) {
                    result.probe = sd_journal_store_probe(
                        kJournalPaths, command.journal.dataset_id);
                }
            }
        }
#endif
    }
    complete_journal_request(command.completion_slot, command.id,
                             observed_generation, result);
    STICKY_LOGI(
        kTag,
        "journal_power=fixture id=%u requested_generation=%u observed_generation=%u result=%u changed=%u",
        static_cast<unsigned>(command.id),
        static_cast<unsigned>(command.journal.mount_generation),
        static_cast<unsigned>(observed_generation),
        static_cast<unsigned>(result.status), result.changed ? 1U : 0U);
}
#endif

#if defined(STICKY_OBJECT_POWER_TEST) && STICKY_OBJECT_POWER_TEST
void complete_object_power_request(
    uint32_t id, uint32_t request_generation,
    uint32_t observed_generation, const SdStorageObjectPowerResult &outcome)
{
    if (s_object_power_completion_id.load(std::memory_order_acquire) != id ||
        static_cast<ObjectPowerCompletionState>(
            s_object_power_completion_state.load(std::memory_order_acquire)) !=
            ObjectPowerCompletionState::Pending) {
        STICKY_LOGE(kTag, "object_power=request id=%u completion=invalid",
                    static_cast<unsigned>(id));
        return;
    }
    s_object_power_result = outcome;
    s_object_power_result.request_id = id;
    s_object_power_result.mount_generation = request_generation;
    s_object_power_result.observed_mount_generation = observed_generation;
    s_object_power_completion_state.store(
        static_cast<uint8_t>(ObjectPowerCompletionState::Completed),
        std::memory_order_release);
}

void process_object_power_request(StickySdCard &card,
                                  const Command &command,
                                  bool write)
{
    SdStorageObjectPowerResult result = {};
    result.service_status = ESP_ERR_INVALID_STATE;
    const uint32_t observed_generation = card.generation();
    const SdManifestProbeResult manifest =
        card.accepts(s_object_power_request_generation, write)
            ? sd_manifest_probe(kManifestSlotAPath, kManifestSlotBPath)
            : SdManifestProbeResult{};
    const bool selected = manifest.state == SdManifestProbeState::Ready ||
                          manifest.state == SdManifestProbeState::Degraded;
    std::array<uint8_t, kObjectPowerFixtureBytes> bytes = {};
    sd_object_power_fixture_bytes(bytes);

    if (!card.accepts(s_object_power_request_generation, write)) {
        result.service_status = card.state() == SdCardState::ReadyReadOnly &&
                                        write
                                    ? ESP_ERR_NOT_ALLOWED
                                    : ESP_ERR_INVALID_STATE;
    } else if (!selected) {
        result.service_status = ESP_ERR_INVALID_STATE;
    } else if (std::strcmp(s_object_power_dataset,
                           manifest.selected.dataset_id) != 0) {
        result.service_status = ESP_ERR_INVALID_ARG;
    } else {
        result.service_status = ESP_OK;
        if (write) {
            StickySdCardSpace space = {};
            if (card.space(space) != ESP_OK) {
                result.service_status = ESP_FAIL;
            } else {
                const SdStorageUsage usage = {space.free_bytes};
                if (sd_storage_check_write(
                        bytes.size(), usage) !=
                    SdStoragePolicyStatus::Ok) {
                    result.service_status = ESP_ERR_NO_MEM;
                }
            }
        }
        if (result.service_status == ESP_OK) {
            if (write) {
#if defined(STICKY_OBJECT_PARTIAL_TEST) && STICKY_OBJECT_PARTIAL_TEST
                const SdRecordObjectWriteResult written =
                    write_object_power_partial_fixture(
                        bytes.data(), bytes.size());
#else
                const SdRecordObjectWriteResult written =
                    sd_record_object_write(
                        kObjectPowerPaths, bytes.data(), bytes.size());
#endif
                result.write_status = written.status;
                result.changed = written.changed;
                result.probe = written.probe;
            } else {
                result.write_status = SdRecordObjectWriteStatus::Ok;
                result.probe = sd_record_object_probe(
                    kObjectPowerPaths.object, bytes.data(), bytes.size());
            }
        }
    }
    complete_object_power_request(
        command.id, s_object_power_request_generation,
        observed_generation, result);
    STICKY_LOGI(
        kTag,
        "object_power=request id=%u generation=%u observed=%u write=%u service=%s status=%u probe=%u bytes=%llu changed=%u",
        static_cast<unsigned>(command.id),
        static_cast<unsigned>(s_object_power_request_generation),
        static_cast<unsigned>(observed_generation), write ? 1U : 0U,
        esp_err_to_name(result.service_status),
        static_cast<unsigned>(result.write_status),
        static_cast<unsigned>(result.probe.status),
        static_cast<unsigned long long>(result.probe.file_bytes),
        result.changed ? 1U : 0U);
}
#endif

esp_err_t mount_if_available(StickySdCard &card)
{
    if (!card.present() || card.mounted()) {
        return ESP_OK;
    }
    const esp_err_t result = card.mount();
    if (result == ESP_OK) {
        STICKY_LOGI(kTag, "card=mount generation=%u result=ok",
                    static_cast<unsigned>(card.generation()));
        const SdManifestProbeResult probe =
            sd_manifest_probe(kManifestSlotAPath, kManifestSlotBPath);
        publish_manifest_status(probe);
        refresh_recovery(probe);
        const esp_err_t capacity_result = refresh_capacity(card);
        if (capacity_result != ESP_OK) {
            STICKY_LOGW(kTag, "card=capacity result=%s",
                        esp_err_to_name(capacity_result));
        }
        switch (probe.state) {
        case SdManifestProbeState::NotInitialized:
            STICKY_LOGI(kTag,
                        "manifest=not_initialized access=read_only_probe");
            break;
        case SdManifestProbeState::Ready:
            STICKY_LOGI(
                kTag, "manifest=ready slot=%c commit=%llu",
                probe.selected_slot == SdManifestSlot::A ? 'A' : 'B',
                static_cast<unsigned long long>(
                    probe.selected.commit_generation));
            break;
        case SdManifestProbeState::Degraded:
            STICKY_LOGW(
                kTag, "manifest=degraded slot=%c commit=%llu",
                probe.selected_slot == SdManifestSlot::A ? 'A' : 'B',
                static_cast<unsigned long long>(
                    probe.selected.commit_generation));
            break;
        case SdManifestProbeState::Corrupt:
            STICKY_LOGW(kTag, "manifest=corrupt action=none");
            break;
        case SdManifestProbeState::IoError:
            STICKY_LOGW(kTag, "manifest=io_error action=none");
            break;
        }
    } else {
        STICKY_LOGW(kTag, "card=mount state=%s result=%s",
                    state_name(card.state()), esp_err_to_name(result));
    }
    publish_card_status(card);
    return result;
}

esp_err_t cleanup_fault(StickySdCard &card)
{
    const esp_err_t result = card.cleanup_fault();
    if (result == ESP_OK) {
        STICKY_LOGI(kTag, "card=cleanup state=%s result=ok",
                    state_name(card.state()));
    } else {
        STICKY_LOGW(kTag, "card=cleanup state=%s result=%s",
                    state_name(card.state()), esp_err_to_name(result));
    }
    publish_card_status(card);
    return result;
}

esp_err_t prepare_sleep(StickySdCard &card)
{
    // Admission is already closed before this FIFO command is queued. Every
    // previously admitted file request has therefore completed before unmount.
    esp_err_t result = ESP_OK;
    if (card.state() == SdCardState::Fault) {
        result = cleanup_fault(card);
    } else if (card.state() == SdCardState::Ejecting && card.mounted()) {
        result = card.finish_eject();
    } else if (card.mounted()) {
        result = card.begin_eject();
        if (result == ESP_OK) {
            result = card.finish_eject();
        }
    }

    if (result == ESP_OK) {
        STICKY_LOGI(kTag, "lifecycle=prepare_sleep state=%s result=ok",
                    state_name(card.state()));
    } else {
        STICKY_LOGE(kTag, "lifecycle=prepare_sleep state=%s result=%s",
                    state_name(card.state()), esp_err_to_name(result));
    }
    publish_card_status(card);
    return result;
}

esp_err_t delete_application_data(StickySdCard &card,
                                  const SdDataDeleteConfirmation &confirmation)
{
    // Poll physical presence and re-read the on-card identity at execution time.
    card.poll();
    if (!card.accepts(confirmation.mount_generation, true))
        return ESP_ERR_INVALID_STATE;
    const auto before = sd_manifest_probe(kManifestSlotAPath, kManifestSlotBPath);
    const bool selected = before.state == SdManifestProbeState::Ready ||
                          before.state == SdManifestProbeState::Degraded;
    if (!confirmation.matches(card.generation(), selected, before.selected.dataset_id))
        return ESP_ERR_INVALID_STATE;
    // Invalidate even when removal later fails: surviving manifests must not
    // make old domain caches valid again under the same generation.
    publish_recovery_status({});
    s_recovery->index.reset();
    card.invalidate_requests();
    publish_card_status(card);
    const SdApplicationDataDeleteResult removed =
        sd_application_data_delete("/sdcard", kApplicationDirectory);
    if (removed.status != SdApplicationDataDeleteStatus::Ok) {
        STICKY_LOGE(kTag, "data_delete=status=%u files=%u dirs=%u",
                    static_cast<unsigned>(removed.status),
                    static_cast<unsigned>(removed.files_removed),
                    static_cast<unsigned>(removed.directories_removed));
    }
    const SdManifestProbeResult probe =
        sd_manifest_probe(kManifestSlotAPath, kManifestSlotBPath);
    refresh_recovery(probe);
    if (removed.status != SdApplicationDataDeleteStatus::Ok)
        publish_recovery_status({}); // Fail closed after partial deletion.
    publish_manifest_status(probe);
    refresh_capacity(card);
    STICKY_LOGI(kTag, "data_delete=finished files=%u dirs=%u",
                static_cast<unsigned>(removed.files_removed),
                static_cast<unsigned>(removed.directories_removed));
    return removed.status == SdApplicationDataDeleteStatus::Ok ? ESP_OK : ESP_FAIL;
}

void complete_command(uint32_t id, esp_err_t result)
{
    s_completed_result.store(static_cast<int>(result),
                             std::memory_order_relaxed);
    s_completed_command_id.store(id, std::memory_order_release);
    s_control_busy.store(false, std::memory_order_release);
}

void storage_task(void *)
{
    StickySdCard &card = sticky_sdcard_device();
    bool sleep_prepared = false;
    uint32_t retry_polls = 0U;
    uint32_t capacity_polls = 0U;
#if defined(STICKY_SD_INJECT_MOUNT_FAILURE_ONCE) && \
    STICKY_SD_INJECT_MOUNT_FAILURE_ONCE
    uint32_t mount_fault_report_polls = 0U;
#endif

    while (true) {
        Command command = {};
        if (xQueueReceive(s_commands, &command, kPollInterval) == pdTRUE) {
            if (command.type == CommandType::Manifest) {
                process_manifest_request(card, command);
                if (card.can_read()) {
                    refresh_capacity(card);
                }
                s_pending_file_requests.fetch_sub(1U,
                                                  std::memory_order_acq_rel);
                continue;
            }
            if (command.type == CommandType::Journal) {
                process_journal_request(card, command);
                if (card.can_read()) {
                    refresh_capacity(card);
                }
                s_pending_file_requests.fetch_sub(1U,
                                                  std::memory_order_acq_rel);
                continue;
            }
            if (command.type == CommandType::Repository) {
                process_repository_request(card, command);
                if (card.can_read()) {
                    refresh_capacity(card);
                }
                s_pending_file_requests.fetch_sub(1U,
                                                  std::memory_order_acq_rel);
                continue;
            }
            if (command.type == CommandType::DeleteApplicationData) {
                const esp_err_t result =
                    s_pending_file_requests.load(std::memory_order_acquire) ==
                            0U
                        ? delete_application_data(card, command.delete_confirmation)
                        : ESP_ERR_INVALID_STATE;
                complete_command(command.id, result);
                continue;
            }
#if defined(STICKY_JOURNAL_POWER_TEST) && STICKY_JOURNAL_POWER_TEST
            if (command.type == CommandType::JournalPowerFixture) {
                process_journal_power_fixture(card, command);
                if (card.can_read()) {
                    refresh_capacity(card);
                }
                s_pending_file_requests.fetch_sub(1U,
                                                  std::memory_order_acq_rel);
                continue;
            }
#endif
#if defined(STICKY_OBJECT_POWER_TEST) && STICKY_OBJECT_POWER_TEST
            if (command.type == CommandType::ObjectPowerProbe ||
                command.type == CommandType::ObjectPowerWrite) {
                process_object_power_request(
                    card, command,
                    command.type == CommandType::ObjectPowerWrite);
                if (card.can_read()) {
                    refresh_capacity(card);
                }
                s_pending_file_requests.fetch_sub(1U,
                                                  std::memory_order_acq_rel);
                continue;
            }
#endif

#if (defined(STICKY_INDEX_POWER_TEST) && STICKY_INDEX_POWER_TEST) || \
    (defined(STICKY_CHECKPOINT_COMMIT_TEST) && \
     STICKY_CHECKPOINT_COMMIT_TEST)
            if (command.type == CommandType::IndexPowerProbe ||
                command.type == CommandType::IndexPowerWrite) {
                process_index_power_request(
                    card, command,
                    command.type == CommandType::IndexPowerWrite);
                if (card.can_read()) {
                    refresh_capacity(card);
                }
                s_pending_file_requests.fetch_sub(1U,
                                                  std::memory_order_acq_rel);
                continue;
            }
#endif

            if (command.type != CommandType::PrepareSleep) {
                // Unknown commands must never fall through to an unmount.
                STICKY_LOGE(kTag, "command=unknown type=%u result=rejected",
                            static_cast<unsigned>(command.type));
                complete_command(command.id, ESP_ERR_NOT_SUPPORTED);
                continue;
            }
            const esp_err_t result =
                s_pending_file_requests.load(std::memory_order_acquire) == 0U
                    ? prepare_sleep(card)
                    : ESP_ERR_INVALID_STATE;
            sleep_prepared = result == ESP_OK;
            complete_command(command.id, result);
            if (!s_sleep_requested.load(std::memory_order_acquire)) {
                sleep_prepared = false;
                if (!s_manual_eject_hold.load(std::memory_order_acquire) &&
                    xSemaphoreTake(s_submit_lock, portMAX_DELAY) == pdTRUE) {
                    open_admission_locked();
                    xSemaphoreGive(s_submit_lock);
                }
            }
            continue;
        }

        if (sleep_prepared &&
            !s_sleep_requested.load(std::memory_order_acquire)) {
            // The caller timed out after the command was queued. Restore normal
            // hot-plug handling instead of leaving an awake device suspended.
            sleep_prepared = false;
            if (!s_manual_eject_hold.load(std::memory_order_acquire) &&
                xSemaphoreTake(s_submit_lock, portMAX_DELAY) == pdTRUE) {
                open_admission_locked();
                xSemaphoreGive(s_submit_lock);
            }
        }
        const StickySdCardPollResult poll = card.poll();
        publish_card_status(card);
        if (poll.event == StickySdCardEvent::Inserted) {
            STICKY_LOGI(kTag, "card=presence event=inserted");
            retry_polls = 0U;
            capacity_polls = 0U;
        } else if (poll.event == StickySdCardEvent::Removed) {
            STICKY_LOGI(kTag, "card=presence event=removed result=ok");
            if (s_manual_eject_hold.exchange(false,
                                             std::memory_order_acq_rel) &&
                xSemaphoreTake(s_submit_lock, portMAX_DELAY) == pdTRUE) {
                open_admission_locked();
                xSemaphoreGive(s_submit_lock);
            }
            capacity_polls = 0U;
        } else if (poll.event == StickySdCardEvent::RemovalCleanupFailed) {
            STICKY_LOGW(kTag,
                        "card=presence event=removed cleanup=%s",
                        esp_err_to_name(poll.error));
            retry_polls = 0U;
            capacity_polls = 0U;
        }

        if (sleep_prepared) {
            continue;
        }
        if (retry_polls > 0U) {
            --retry_polls;
            continue;
        }

        if (card.state() == SdCardState::Fault) {
            const esp_err_t result = card.mounted() && !card.present()
                                         ? card.recover_removed_card()
                                         : cleanup_fault(card);
            publish_card_status(card);
            retry_polls = result == ESP_OK ? 0U : kRetryPollCount;
            continue;
        }
        if (card.present() && !card.mounted() &&
            !s_manual_eject_hold.load(std::memory_order_acquire)) {
            const esp_err_t result = mount_if_available(card);
            retry_polls = result == ESP_OK ? 0U : kRetryPollCount;
            capacity_polls = 0U;
            continue;
        }
        if (card.can_read() && ++capacity_polls >= kCapacityPollCount) {
            refresh_capacity(card);
            capacity_polls = 0U;
        }
#if defined(STICKY_SD_INJECT_MOUNT_FAILURE_ONCE) && \
    STICKY_SD_INJECT_MOUNT_FAILURE_ONCE
        if (++mount_fault_report_polls >= 50U) {
            const bool recovered = card.can_read() && card.generation() >= 2U;
            STICKY_LOGI(
                kTag,
                "fault_test=mount_once state=%s generation=%u result=%s",
                state_name(card.state()),
                static_cast<unsigned>(card.generation()),
                recovered ? "recovered" : "pending");
            mount_fault_report_polls = 0U;
        }
#endif
    }
}

uint32_t next_command_id()
{
    uint32_t id = s_next_command_id.fetch_add(1U,
                                               std::memory_order_relaxed) + 1U;
    if (id == 0U) {
        id = s_next_command_id.fetch_add(1U,
                                         std::memory_order_relaxed) + 1U;
    }
    return id;
}

esp_err_t submit_manifest_request(const SdManifestRequest &request,
                                  SdStorageRequestHandle &handle)
{
    handle = {};
    if (!s_running.load(std::memory_order_acquire) || s_commands == nullptr ||
        s_submit_lock == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_submit_lock, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    if (!s_accepting_manifest_requests.load(std::memory_order_acquire)) {
        xSemaphoreGive(s_submit_lock);
        return ESP_ERR_INVALID_STATE;
    }

    const uint32_t id = next_command_id();
    SdManifestCompletionToken completion = {};
    if (!s_manifest_completions.reserve(id, request.mount_generation,
                                        completion)) {
        xSemaphoreGive(s_submit_lock);
        return ESP_ERR_NO_MEM;
    }

    Command command = {};
    command.type = CommandType::Manifest;
    command.id = id;
    command.completion_slot = completion.slot;
    command.manifest = request;
    s_pending_file_requests.fetch_add(1U, std::memory_order_acq_rel);
    if (xQueueSend(s_commands, &command, 0) != pdTRUE) {
        s_pending_file_requests.fetch_sub(1U, std::memory_order_acq_rel);
        s_manifest_completions.cancel(completion);
        xSemaphoreGive(s_submit_lock);
        return ESP_ERR_TIMEOUT;
    }

    handle.id = id;
    handle.slot = completion.slot;
    xSemaphoreGive(s_submit_lock);
    return ESP_OK;
}

esp_err_t submit_journal_request_as(const SdJournalRequest &request,
                                    CommandType type,
                                    SdStorageRequestHandle &handle)
{
    handle = {};
    if (!s_running.load(std::memory_order_acquire) || s_commands == nullptr ||
        s_submit_lock == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_submit_lock, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    if (!s_accepting_journal_requests.load(std::memory_order_acquire)) {
        xSemaphoreGive(s_submit_lock);
        return ESP_ERR_INVALID_STATE;
    }

    const uint32_t id = next_command_id();
    SdJournalCompletionToken completion = {};
    if (!s_journal_completions.reserve(id, request.mount_generation,
                                       completion)) {
        xSemaphoreGive(s_submit_lock);
        return ESP_ERR_NO_MEM;
    }

    Command command = {};
    command.type = type;
    command.id = id;
    command.completion_slot = completion.slot;
    command.journal = request;
    s_pending_file_requests.fetch_add(1U, std::memory_order_acq_rel);
    if (xQueueSend(s_commands, &command, 0) != pdTRUE) {
        s_pending_file_requests.fetch_sub(1U, std::memory_order_acq_rel);
        s_journal_completions.cancel(completion);
        xSemaphoreGive(s_submit_lock);
        return ESP_ERR_TIMEOUT;
    }

    handle.id = id;
    handle.slot = completion.slot;
    xSemaphoreGive(s_submit_lock);
    return ESP_OK;
}

esp_err_t submit_journal_request(const SdJournalRequest &request,
                                 SdStorageRequestHandle &handle)
{
    return submit_journal_request_as(request, CommandType::Journal, handle);
}

esp_err_t submit_repository_request(
    SdRepositoryRequestOperation operation, uint32_t mount_generation,
    const char *dataset_id, const RepositoryKey *key,
    const RepositoryPageRequest *page, const RepositoryMutation *mutation,
    uint32_t expected_revision, uint64_t transaction_id,
    SdStorageRequestHandle &handle)
{
    handle = {};
    if (!s_running.load(std::memory_order_acquire) || s_commands == nullptr ||
        s_submit_lock == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_submit_lock, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    if (!s_accepting_repository_requests.load(std::memory_order_acquire)) {
        xSemaphoreGive(s_submit_lock);
        return ESP_ERR_INVALID_STATE;
    }

    const uint32_t id = next_command_id();
    SdRepositoryCompletionToken completion = {};
    bool reserved = false;
    switch (operation) {
    case SdRepositoryRequestOperation::Read:
        reserved = key != nullptr && s_repository_completions.reserve_read(
            id, mount_generation, dataset_id, *key, completion);
        break;
    case SdRepositoryRequestOperation::List:
        reserved = page != nullptr && s_repository_completions.reserve_list(
            id, mount_generation, dataset_id, *page, completion);
        break;
    case SdRepositoryRequestOperation::Commit:
        reserved = mutation != nullptr &&
                   s_repository_completions.reserve_commit(
                       id, mount_generation, dataset_id, *mutation,
                       transaction_id, completion);
        break;
    case SdRepositoryRequestOperation::Erase:
        reserved = key != nullptr && s_repository_completions.reserve_erase(
            id, mount_generation, dataset_id, *key, expected_revision,
            transaction_id, completion);
        break;
    }
    if (!reserved) {
        xSemaphoreGive(s_submit_lock);
        return ESP_ERR_NO_MEM;
    }

    Command command = {};
    command.type = CommandType::Repository;
    command.id = id;
    command.completion_slot = completion.slot;
    s_pending_file_requests.fetch_add(1U, std::memory_order_acq_rel);
    if (xQueueSend(s_commands, &command, 0) != pdTRUE) {
        s_pending_file_requests.fetch_sub(1U, std::memory_order_acq_rel);
        s_repository_completions.cancel(completion);
        xSemaphoreGive(s_submit_lock);
        return ESP_ERR_TIMEOUT;
    }
    handle.id = id;
    handle.slot = completion.slot;
    xSemaphoreGive(s_submit_lock);
    return ESP_OK;
}

#if defined(STICKY_OBJECT_POWER_TEST) && STICKY_OBJECT_POWER_TEST
esp_err_t submit_object_power_request(
    CommandType type, uint32_t mount_generation, const char *dataset_id,
    SdStorageRequestHandle &handle)
{
    handle = {};
    if (mount_generation == 0U || !sd_dataset_id_valid(dataset_id)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_running.load(std::memory_order_acquire) || s_commands == nullptr ||
        s_submit_lock == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_submit_lock, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    if (!s_accepting_journal_requests.load(std::memory_order_acquire) ||
        static_cast<ObjectPowerCompletionState>(
            s_object_power_completion_state.load(std::memory_order_acquire)) !=
            ObjectPowerCompletionState::Free) {
        xSemaphoreGive(s_submit_lock);
        return ESP_ERR_INVALID_STATE;
    }

    const uint32_t id = next_command_id();
    s_object_power_completion_id.store(id, std::memory_order_relaxed);
    s_object_power_request_generation = mount_generation;
    std::memcpy(s_object_power_dataset, dataset_id,
                sizeof(s_object_power_dataset));
    s_object_power_result = {};
    s_object_power_completion_state.store(
        static_cast<uint8_t>(ObjectPowerCompletionState::Pending),
        std::memory_order_release);

    Command command = {};
    command.type = type;
    command.id = id;
    s_pending_file_requests.fetch_add(1U, std::memory_order_acq_rel);
    if (xQueueSend(s_commands, &command, 0) != pdTRUE) {
        s_pending_file_requests.fetch_sub(1U, std::memory_order_acq_rel);
        s_object_power_completion_id.store(0U, std::memory_order_relaxed);
        s_object_power_request_generation = 0U;
        s_object_power_dataset[0] = '\0';
        s_object_power_completion_state.store(
            static_cast<uint8_t>(ObjectPowerCompletionState::Free),
            std::memory_order_release);
        xSemaphoreGive(s_submit_lock);
        return ESP_ERR_TIMEOUT;
    }
    handle.id = id;
    handle.slot = 0U;
    xSemaphoreGive(s_submit_lock);
    return ESP_OK;
}
#endif

#if (defined(STICKY_INDEX_POWER_TEST) && STICKY_INDEX_POWER_TEST) || \
    (defined(STICKY_CHECKPOINT_COMMIT_TEST) && \
     STICKY_CHECKPOINT_COMMIT_TEST)
esp_err_t submit_index_power_request(
    CommandType type, uint32_t mount_generation, const char *dataset_id,
    uint64_t last_transaction_id, SdStorageRequestHandle &handle)
{
    handle = {};
    if (mount_generation == 0U || last_transaction_id == 0U ||
        !sd_dataset_id_valid(dataset_id)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_running.load(std::memory_order_acquire) || s_commands == nullptr ||
        s_submit_lock == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_submit_lock, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    if (!s_accepting_journal_requests.load(std::memory_order_acquire) ||
        static_cast<IndexPowerCompletionState>(
            s_index_power_completion_state.load(std::memory_order_acquire)) !=
            IndexPowerCompletionState::Free) {
        xSemaphoreGive(s_submit_lock);
        return ESP_ERR_INVALID_STATE;
    }

    const uint32_t id = next_command_id();
    s_index_power_completion_id.store(id, std::memory_order_relaxed);
    s_index_power_request_generation = mount_generation;
    s_index_power_last_transaction = last_transaction_id;
    std::memcpy(s_index_power_dataset, dataset_id,
                sizeof(s_index_power_dataset));
    s_index_power_result = {};
    s_index_power_completion_state.store(
        static_cast<uint8_t>(IndexPowerCompletionState::Pending),
        std::memory_order_release);

    Command command = {};
    command.type = type;
    command.id = id;
    s_pending_file_requests.fetch_add(1U, std::memory_order_acq_rel);
    if (xQueueSend(s_commands, &command, 0) != pdTRUE) {
        s_pending_file_requests.fetch_sub(1U, std::memory_order_acq_rel);
        s_index_power_completion_id.store(0U, std::memory_order_relaxed);
        s_index_power_request_generation = 0U;
        s_index_power_last_transaction = 0U;
        s_index_power_dataset[0] = '\0';
        s_index_power_completion_state.store(
            static_cast<uint8_t>(IndexPowerCompletionState::Free),
            std::memory_order_release);
        xSemaphoreGive(s_submit_lock);
        return ESP_ERR_TIMEOUT;
    }
    handle.id = id;
    handle.slot = 0U;
    xSemaphoreGive(s_submit_lock);
    return ESP_OK;
}
#endif

}  // namespace

esp_err_t sd_storage_service_start()
{
    if (s_running.load(std::memory_order_acquire)) {
        return ESP_OK;
    }

    app_log_register_tag(kTag);
    if (s_recovery == nullptr) {
        void *memory = heap_caps_malloc(sizeof(RecoveryState),
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (memory == nullptr) return ESP_ERR_NO_MEM;
        s_recovery = new (memory) RecoveryState{};
        STICKY_LOGI(kTag, "recovery=allocated psram_bytes=%u slots=%u",
                    static_cast<unsigned>(sizeof(RecoveryState)),
                    static_cast<unsigned>(kSdRecoveryRecordCapacity));
    }
    s_commands = xQueueCreate(kStorageQueueDepth, sizeof(Command));
    if (s_commands == nullptr) {
        return ESP_ERR_NO_MEM;
    }
    s_submit_lock = xSemaphoreCreateMutex();
    if (s_submit_lock == nullptr) {
        vQueueDelete(s_commands);
        s_commands = nullptr;
        return ESP_ERR_NO_MEM;
    }
    open_admission_locked();
    if (xTaskCreate(storage_task, "sd_storage", kTaskStackSize, nullptr,
                    kTaskPriority, &s_task) != pdPASS) {
        s_accepting_manifest_requests.store(false,
                                             std::memory_order_release);
        s_accepting_journal_requests.store(false,
                                           std::memory_order_release);
        s_accepting_repository_requests.store(false,
                                              std::memory_order_release);
        s_accepting_work.store(false, std::memory_order_release);
        vSemaphoreDelete(s_submit_lock);
        s_submit_lock = nullptr;
        vQueueDelete(s_commands);
        s_commands = nullptr;
        s_task = nullptr;
        return ESP_ERR_NO_MEM;
    }
    s_running.store(true, std::memory_order_release);
    STICKY_LOGI(kTag, "lifecycle=start queue_depth=%u result=ok",
                static_cast<unsigned>(kStorageQueueDepth));
    return ESP_OK;
}

SdStorageServiceStatus sd_storage_service_status()
{
    SdStorageServiceStatus status = {};
    uint32_t before = 0U;
    uint32_t after = 0U;
    do {
        before = s_status_revision.load(std::memory_order_acquire);
        if ((before & 1U) != 0U) {
            vTaskDelay(1);
            continue;
        }
        status.card_state = static_cast<SdCardState>(
            s_card_state.load(std::memory_order_relaxed));
        status.mount_generation =
            s_card_generation.load(std::memory_order_relaxed);
        status.capacity_known =
            s_capacity_known.load(std::memory_order_relaxed);
        status.total_bytes = s_total_bytes.load(std::memory_order_relaxed);
        status.free_bytes = s_free_bytes.load(std::memory_order_relaxed);
        status.manifest_known =
            s_manifest_known.load(std::memory_order_relaxed);
        status.manifest_state = static_cast<SdManifestProbeState>(
            s_manifest_state.load(std::memory_order_relaxed));
        portENTER_CRITICAL(&s_manifest_snapshot_lock);
        status.manifest_selected = s_manifest_selected;
        status.selected_manifest = s_selected_manifest;
        status.selected_slot = s_selected_slot;
        portEXIT_CRITICAL(&s_manifest_snapshot_lock);
        status.recovery_known =
            s_recovery_known.load(std::memory_order_relaxed);
        status.recovery_status = static_cast<SdRecoveryLoadStatus>(
            s_recovery_status.load(std::memory_order_relaxed));
        status.recovery_view_ready =
            s_recovery_view_ready.load(std::memory_order_relaxed);
        status.recovery_checkpoint_used =
            s_recovery_checkpoint_used.load(std::memory_order_relaxed);
        status.recovery_journal_records =
            s_recovery_journal_records.load(std::memory_order_relaxed);
        status.recovery_last_transaction_id =
            s_recovery_last_transaction_id.load(std::memory_order_relaxed);
        status.recovery_snapshot_transaction_id =
            s_recovery_snapshot_transaction_id.load(
                std::memory_order_relaxed);
        status.recovery_live_records =
            s_recovery_live_records.load(std::memory_order_relaxed);
        status.recovery_verified_objects =
            s_recovery_verified_objects.load(std::memory_order_relaxed);
        after = s_status_revision.load(std::memory_order_acquire);
    } while (before != after || (after & 1U) != 0U);
    status.running = s_running.load(std::memory_order_acquire);
    status.accepting_manifest_requests =
        s_accepting_manifest_requests.load(std::memory_order_acquire);
    status.accepting_journal_requests =
        s_accepting_journal_requests.load(std::memory_order_acquire);
    status.accepting_repository_requests =
        s_accepting_repository_requests.load(std::memory_order_acquire);
    status.accepting_work =
        s_accepting_work.load(std::memory_order_acquire);
    status.pending_work = s_pending_work.load(std::memory_order_acquire);
    return status;
}

esp_err_t sd_storage_service_submit_manifest_initialize(
    uint32_t mount_generation, const char *dataset_id, bool user_confirmed,
    SdStorageRequestHandle &handle)
{
    SdManifestRequest request = {};
    if (!sd_manifest_make_initialize_request(
            mount_generation, dataset_id, user_confirmed, request)) {
        handle = {};
        return ESP_ERR_INVALID_ARG;
    }
    return submit_manifest_request(request, handle);
}

esp_err_t sd_storage_service_submit_manifest_commit(
    uint32_t mount_generation, const SdManifest &manifest,
    SdStorageRequestHandle &handle)
{
    SdManifestRequest request = {};
    if (!sd_manifest_make_commit_request(mount_generation, manifest,
                                         request)) {
        handle = {};
        return ESP_ERR_INVALID_ARG;
    }
    return submit_manifest_request(request, handle);
}

esp_err_t sd_storage_service_collect_manifest_result(
    const SdStorageRequestHandle &handle, SdStorageManifestResult &result)
{
    result = {};
    if (handle.id == 0U || handle.slot >= s_manifest_completions.capacity()) {
        return ESP_ERR_INVALID_ARG;
    }

    const SdManifestCompletionToken token = {handle.id, handle.slot};
    SdManifestCompletionRecord record = {};
    const SdManifestCompletionCollectStatus status =
        s_manifest_completions.collect(token, record);
    if (status == SdManifestCompletionCollectStatus::Pending) {
        return ESP_ERR_NOT_FINISHED;
    }
    if (status == SdManifestCompletionCollectStatus::InvalidToken) {
        return ESP_ERR_INVALID_ARG;
    }
    if (status != SdManifestCompletionCollectStatus::Ok) {
        return ESP_ERR_INVALID_STATE;
    }

    result.request_id = record.request_id;
    result.mount_generation = record.request_mount_generation;
    result.observed_mount_generation = record.observed_mount_generation;
    result.outcome = record.outcome;
    return ESP_OK;
}

esp_err_t sd_storage_service_submit_journal_probe(
    uint32_t mount_generation, const char *dataset_id,
    SdStorageRequestHandle &handle)
{
    SdJournalRequest request = {};
    if (!sd_journal_make_probe_request(mount_generation, dataset_id,
                                       request)) {
        handle = {};
        return ESP_ERR_INVALID_ARG;
    }
    return submit_journal_request(request, handle);
}

esp_err_t sd_storage_service_submit_journal_append(
    uint32_t mount_generation, const char *dataset_id,
    SdJournalRecordType type, uint64_t transaction_id,
    const uint8_t *payload, size_t payload_size,
    SdStorageRequestHandle &handle)
{
    SdJournalRequest request = {};
    if (!sd_journal_make_append_request(
            mount_generation, dataset_id, type, transaction_id, payload,
            payload_size, request)) {
        handle = {};
        return ESP_ERR_INVALID_ARG;
    }
    return submit_journal_request(request, handle);
}

esp_err_t sd_storage_service_collect_journal_result(
    const SdStorageRequestHandle &handle, SdStorageJournalResult &result)
{
    result = {};
    if (handle.id == 0U || handle.slot >= s_journal_completions.capacity()) {
        return ESP_ERR_INVALID_ARG;
    }
    const SdJournalCompletionToken token = {handle.id, handle.slot};
    SdJournalCompletionRecord record = {};
    const SdJournalCompletionCollectStatus status =
        s_journal_completions.collect(token, record);
    if (status == SdJournalCompletionCollectStatus::Pending) {
        return ESP_ERR_NOT_FINISHED;
    }
    if (status == SdJournalCompletionCollectStatus::InvalidToken) {
        return ESP_ERR_INVALID_ARG;
    }
    if (status != SdJournalCompletionCollectStatus::Ok) {
        return ESP_ERR_INVALID_STATE;
    }
    result.request_id = record.request_id;
    result.mount_generation = record.request_mount_generation;
    result.observed_mount_generation = record.observed_mount_generation;
    result.outcome = record.outcome;
    return ESP_OK;
}

esp_err_t sd_storage_service_submit_repository_read(
    uint32_t mount_generation, const char *dataset_id,
    const RepositoryKey &key, SdStorageRequestHandle &handle)
{
    if (mount_generation == 0U || !sd_dataset_id_valid(dataset_id) ||
        !repository_key_valid(key)) {
        handle = {};
        return ESP_ERR_INVALID_ARG;
    }
    return submit_repository_request(
        SdRepositoryRequestOperation::Read, mount_generation, dataset_id,
        &key, nullptr, nullptr, 0U, 0U, handle);
}

esp_err_t sd_storage_service_submit_repository_list(
    uint32_t mount_generation, const char *dataset_id,
    const RepositoryPageRequest &page, SdStorageRequestHandle &handle)
{
    if (mount_generation == 0U || !sd_dataset_id_valid(dataset_id) ||
        !repository_kind_valid(page.repository) || page.limit == 0U ||
        page.limit > kRepositoryPageEntries) {
        handle = {};
        return ESP_ERR_INVALID_ARG;
    }
    return submit_repository_request(
        SdRepositoryRequestOperation::List, mount_generation, dataset_id,
        nullptr, &page, nullptr, 0U, 0U, handle);
}

esp_err_t sd_storage_service_submit_repository_commit(
    uint32_t mount_generation, const char *dataset_id,
    const RepositoryMutation &mutation, uint64_t transaction_id,
    SdStorageRequestHandle &handle)
{
    if (mount_generation == 0U || !sd_dataset_id_valid(dataset_id) ||
        mutation.type != RepositoryMutationType::Upsert ||
        !repository_key_valid(mutation.key) ||
        mutation.expected_revision == UINT32_MAX ||
        mutation.payload_size == 0U ||
        mutation.payload_size > mutation.payload.size()) {
        handle = {};
        return ESP_ERR_INVALID_ARG;
    }
    return submit_repository_request(
        SdRepositoryRequestOperation::Commit, mount_generation, dataset_id,
        nullptr, nullptr, &mutation, 0U, transaction_id, handle);
}

esp_err_t sd_storage_service_submit_repository_erase(
    uint32_t mount_generation, const char *dataset_id,
    const RepositoryKey &key, uint32_t expected_revision,
    uint64_t transaction_id, SdStorageRequestHandle &handle)
{
    if (mount_generation == 0U || !sd_dataset_id_valid(dataset_id) ||
        !repository_key_valid(key) || expected_revision == 0U ||
        expected_revision == UINT32_MAX) {
        handle = {};
        return ESP_ERR_INVALID_ARG;
    }
    return submit_repository_request(
        SdRepositoryRequestOperation::Erase, mount_generation, dataset_id,
        &key, nullptr, nullptr, expected_revision, transaction_id, handle);
}

esp_err_t sd_storage_service_collect_repository_result(
    const SdStorageRequestHandle &handle,
    SdStorageRepositoryResult &result)
{
    result = {};
    if (handle.id == 0U ||
        handle.slot >= s_repository_completions.capacity()) {
        return ESP_ERR_INVALID_ARG;
    }
    const SdRepositoryCompletionToken token = {handle.id, handle.slot};
    const SdRepositoryCompletionCollectStatus status =
        s_repository_completions.collect(
            token, result.request_id, result.mount_generation,
            result.observed_mount_generation, result.outcome);
    if (status == SdRepositoryCompletionCollectStatus::Pending) {
        return ESP_ERR_NOT_FINISHED;
    }
    if (status == SdRepositoryCompletionCollectStatus::InvalidToken) {
        return ESP_ERR_INVALID_ARG;
    }
    if (status != SdRepositoryCompletionCollectStatus::Ok) {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

#if defined(STICKY_JOURNAL_POWER_TEST) && STICKY_JOURNAL_POWER_TEST
esp_err_t sd_storage_service_submit_journal_power_fixture(
    uint32_t mount_generation, const char *dataset_id,
    SdStorageRequestHandle &handle)
{
    SdJournalRequest request = {};
    if (!sd_journal_make_probe_request(mount_generation, dataset_id,
                                       request)) {
        handle = {};
        return ESP_ERR_INVALID_ARG;
    }
    return submit_journal_request_as(
        request, CommandType::JournalPowerFixture, handle);
}
#endif

#if defined(STICKY_OBJECT_POWER_TEST) && STICKY_OBJECT_POWER_TEST
esp_err_t sd_storage_service_submit_object_power_probe(
    uint32_t mount_generation, const char *dataset_id,
    SdStorageRequestHandle &handle)
{
    return submit_object_power_request(
        CommandType::ObjectPowerProbe, mount_generation, dataset_id, handle);
}

esp_err_t sd_storage_service_submit_object_power_write(
    uint32_t mount_generation, const char *dataset_id,
    SdStorageRequestHandle &handle)
{
    return submit_object_power_request(
        CommandType::ObjectPowerWrite, mount_generation, dataset_id, handle);
}

esp_err_t sd_storage_service_collect_object_power_result(
    const SdStorageRequestHandle &handle,
    SdStorageObjectPowerResult &result)
{
    result = {};
    if (handle.id == 0U || handle.slot != 0U ||
        s_object_power_completion_id.load(std::memory_order_acquire) !=
            handle.id) {
        return ESP_ERR_INVALID_ARG;
    }
    const ObjectPowerCompletionState state =
        static_cast<ObjectPowerCompletionState>(
            s_object_power_completion_state.load(std::memory_order_acquire));
    if (state == ObjectPowerCompletionState::Pending) {
        return ESP_ERR_NOT_FINISHED;
    }
    if (state != ObjectPowerCompletionState::Completed) {
        return ESP_ERR_INVALID_STATE;
    }
    result = s_object_power_result;
    s_object_power_completion_id.store(0U, std::memory_order_relaxed);
    s_object_power_request_generation = 0U;
    s_object_power_dataset[0] = '\0';
    s_object_power_result = {};
    s_object_power_completion_state.store(
        static_cast<uint8_t>(ObjectPowerCompletionState::Free),
        std::memory_order_release);
    return ESP_OK;
}
#endif

#if (defined(STICKY_INDEX_POWER_TEST) && STICKY_INDEX_POWER_TEST) || \
    (defined(STICKY_CHECKPOINT_COMMIT_TEST) && \
     STICKY_CHECKPOINT_COMMIT_TEST)
esp_err_t sd_storage_service_submit_index_power_probe(
    uint32_t mount_generation, const char *dataset_id,
    uint64_t last_transaction_id, SdStorageRequestHandle &handle)
{
    return submit_index_power_request(
        CommandType::IndexPowerProbe, mount_generation, dataset_id,
        last_transaction_id, handle);
}

esp_err_t sd_storage_service_submit_index_power_write(
    uint32_t mount_generation, const char *dataset_id,
    uint64_t last_transaction_id, SdStorageRequestHandle &handle)
{
    return submit_index_power_request(
        CommandType::IndexPowerWrite, mount_generation, dataset_id,
        last_transaction_id, handle);
}

esp_err_t sd_storage_service_collect_index_power_result(
    const SdStorageRequestHandle &handle,
    SdStorageIndexPowerResult &result)
{
    result = {};
    if (handle.id == 0U || handle.slot != 0U ||
        s_index_power_completion_id.load(std::memory_order_acquire) !=
            handle.id) {
        return ESP_ERR_INVALID_ARG;
    }
    const IndexPowerCompletionState state =
        static_cast<IndexPowerCompletionState>(
            s_index_power_completion_state.load(std::memory_order_acquire));
    if (state == IndexPowerCompletionState::Pending) {
        return ESP_ERR_NOT_FINISHED;
    }
    if (state != IndexPowerCompletionState::Completed) {
        return ESP_ERR_INVALID_STATE;
    }
    result = s_index_power_result;
    s_index_power_completion_id.store(0U, std::memory_order_relaxed);
    s_index_power_request_generation = 0U;
    s_index_power_last_transaction = 0U;
    s_index_power_dataset[0] = '\0';
    s_index_power_result = {};
    s_index_power_completion_state.store(
        static_cast<uint8_t>(IndexPowerCompletionState::Free),
        std::memory_order_release);
    return ESP_OK;
}
#endif

SdStorageWorkStatus sd_storage_service_begin_work(
    SdStorageWorkKind kind, uint32_t mount_generation,
    const char *dataset_id, SdStorageWorkToken &token)
{
    token = {};
    SdDatasetBinding binding = {};
    if (!sd_dataset_binding_make(mount_generation, dataset_id, binding)) {
        return SdStorageWorkStatus::InvalidArgument;
    }
    if (!s_running.load(std::memory_order_acquire) || s_submit_lock == nullptr) {
        return SdStorageWorkStatus::CardUnavailable;
    }
    if (xSemaphoreTake(s_submit_lock, 0) != pdTRUE) {
        return SdStorageWorkStatus::Busy;
    }
    if (!s_accepting_work.load(std::memory_order_acquire)) {
        xSemaphoreGive(s_submit_lock);
        return SdStorageWorkStatus::AdmissionClosed;
    }
    const SdCardState card_state = static_cast<SdCardState>(
        s_card_state.load(std::memory_order_acquire));
    if (card_state == SdCardState::ReadyReadOnly) {
        xSemaphoreGive(s_submit_lock);
        return SdStorageWorkStatus::CardReadOnly;
    }
    if (card_state == SdCardState::Full) {
        xSemaphoreGive(s_submit_lock);
        return SdStorageWorkStatus::NoSpace;
    }
    if (card_state != SdCardState::ReadyReadWrite) {
        xSemaphoreGive(s_submit_lock);
        return SdStorageWorkStatus::CardUnavailable;
    }
    uint32_t current_generation = 0U;
    char current_dataset[kDatasetIdCharacters + 1U] = {};
    if (!selected_dataset_snapshot(current_generation, current_dataset)) {
        xSemaphoreGive(s_submit_lock);
        return SdStorageWorkStatus::ManifestUnavailable;
    }
    const SdStorageWorkStatus status = s_work_gate.admit(
        kind, binding, current_generation, current_dataset,
        next_command_id(), token);
    s_pending_work.store(static_cast<uint32_t>(s_work_gate.pending()),
                         std::memory_order_release);
    xSemaphoreGive(s_submit_lock);
    return status;
}

SdStorageWorkStatus sd_storage_service_finish_work(
    const SdStorageWorkToken &token, SdStorageWorkRecord &record)
{
    record = {};
    if (s_submit_lock == nullptr) {
        return SdStorageWorkStatus::CardUnavailable;
    }
    if (xSemaphoreTake(s_submit_lock, 0) != pdTRUE) {
        return SdStorageWorkStatus::Busy;
    }
    uint32_t current_generation = 0U;
    char current_dataset[kDatasetIdCharacters + 1U] = {};
    SdStorageWorkStatus status = SdStorageWorkStatus::CardUnavailable;
    const SdCardState card_state = static_cast<SdCardState>(
        s_card_state.load(std::memory_order_acquire));
    if (card_state != SdCardState::ReadyReadWrite ||
        !selected_dataset_snapshot(current_generation, current_dataset)) {
        if (!s_work_gate.cancel(token, record)) {
            status = SdStorageWorkStatus::InvalidToken;
        }
    } else {
        status = s_work_gate.finish(token, current_generation,
                                    current_dataset, record);
    }
    s_pending_work.store(static_cast<uint32_t>(s_work_gate.pending()),
                         std::memory_order_release);
    xSemaphoreGive(s_submit_lock);
    return status;
}

bool sd_storage_service_cancel_work(const SdStorageWorkToken &token,
                                    SdStorageWorkRecord &record)
{
    record = {};
    if (s_submit_lock == nullptr ||
        xSemaphoreTake(s_submit_lock, 0) != pdTRUE) {
        return false;
    }
    const bool cancelled = s_work_gate.cancel(token, record);
    s_pending_work.store(static_cast<uint32_t>(s_work_gate.pending()),
                         std::memory_order_release);
    xSemaphoreGive(s_submit_lock);
    return cancelled;
}

SdStoragePolicyStatus sd_storage_service_check_write(uint64_t requested_bytes)
{
    const SdStorageUsage validation_usage = {UINT64_MAX};
    const SdStoragePolicyStatus arguments =
        sd_storage_check_write(requested_bytes, validation_usage);
    if (arguments == SdStoragePolicyStatus::InvalidArgument) {
        return arguments;
    }

    const SdStorageServiceStatus status = sd_storage_service_status();
    if (status.card_state == SdCardState::ReadyReadOnly) {
        return SdStoragePolicyStatus::CardReadOnly;
    }
    if (status.card_state == SdCardState::Full) {
        return SdStoragePolicyStatus::NoSpace;
    }
    if (status.card_state != SdCardState::ReadyReadWrite) {
        return SdStoragePolicyStatus::CardUnavailable;
    }
    if (!status.capacity_known) {
        return SdStoragePolicyStatus::CapacityUnavailable;
    }

    const SdStorageUsage usage = {status.free_bytes};
    return sd_storage_check_write(requested_bytes, usage);
}

esp_err_t sd_storage_service_prepare_sleep(uint32_t timeout_ms)
{
    if (!s_running.load(std::memory_order_acquire) || s_task == nullptr ||
        s_commands == nullptr || s_submit_lock == nullptr) {
        // No service means it never mounted or powered the card.
        return ESP_OK;
    }

    const TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    const TickType_t started = xTaskGetTickCount();
    bool expected = false;
    if (!s_control_busy.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_submit_lock, timeout_ticks) != pdTRUE) {
        s_control_busy.store(false, std::memory_order_release);
        return ESP_ERR_TIMEOUT;
    }

    close_admission_locked();
    s_sleep_requested.store(true, std::memory_order_release);
    xSemaphoreGive(s_submit_lock);

    while (s_pending_work.load(std::memory_order_acquire) != 0U) {
        if (xTaskGetTickCount() - started >= timeout_ticks) {
            s_sleep_requested.store(false, std::memory_order_release);
            if (xSemaphoreTake(s_submit_lock, portMAX_DELAY) == pdTRUE) {
                open_admission_locked();
                xSemaphoreGive(s_submit_lock);
            }
            s_control_busy.store(false, std::memory_order_release);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    const TickType_t elapsed = xTaskGetTickCount() - started;
    const TickType_t remaining = elapsed < timeout_ticks
                                     ? timeout_ticks - elapsed
                                     : 0U;
    if (xSemaphoreTake(s_submit_lock, remaining) != pdTRUE) {
        s_sleep_requested.store(false, std::memory_order_release);
        if (xSemaphoreTake(s_submit_lock, portMAX_DELAY) == pdTRUE) {
            open_admission_locked();
            xSemaphoreGive(s_submit_lock);
        }
        s_control_busy.store(false, std::memory_order_release);
        return ESP_ERR_TIMEOUT;
    }

    const uint32_t id = next_command_id();
    Command command = {};
    command.type = CommandType::PrepareSleep;
    command.id = id;
    if (xQueueSend(s_commands, &command, 0) != pdTRUE) {
        s_sleep_requested.store(false, std::memory_order_release);
        open_admission_locked();
        xSemaphoreGive(s_submit_lock);
        s_control_busy.store(false, std::memory_order_release);
        return ESP_ERR_TIMEOUT;
    }
    xSemaphoreGive(s_submit_lock);

    while (s_completed_command_id.load(std::memory_order_acquire) != id) {
        if (xTaskGetTickCount() - started >= timeout_ticks) {
            s_sleep_requested.store(false, std::memory_order_release);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    const esp_err_t result = static_cast<esp_err_t>(
        s_completed_result.load(std::memory_order_relaxed));
    if (result != ESP_OK) {
        s_sleep_requested.store(false, std::memory_order_release);
        if (xSemaphoreTake(s_submit_lock, portMAX_DELAY) == pdTRUE) {
            open_admission_locked();
            xSemaphoreGive(s_submit_lock);
        }
    }
    return result;
}

esp_err_t sd_storage_service_safe_eject(uint32_t timeout_ms)
{
    if (s_manual_eject_hold.load(std::memory_order_acquire)) return ESP_OK;
    s_manual_eject_hold.store(true, std::memory_order_release);
    const esp_err_t result = sd_storage_service_prepare_sleep(timeout_ms);
    // Wake the storage task without reopening admission. The manual hold keeps
    // the card unmounted; physical removal clears the hold and reopens normal
    // hot-plug handling.
    s_sleep_requested.store(false, std::memory_order_release);
    if (result != ESP_OK) {
        s_manual_eject_hold.store(false, std::memory_order_release);
        if (s_submit_lock != nullptr &&
            xSemaphoreTake(s_submit_lock, portMAX_DELAY) == pdTRUE) {
            open_admission_locked();
            xSemaphoreGive(s_submit_lock);
        }
    }
    return result;
}

esp_err_t sd_storage_service_delete_application_data(
    const SdDataDeleteConfirmation &confirmation, bool user_confirmed,
                                                      uint32_t timeout_ms)
{
    if (!user_confirmed || confirmation.mount_generation == 0U)
        return ESP_ERR_INVALID_ARG;
    if (!s_running.load(std::memory_order_acquire) || s_task == nullptr ||
        s_commands == nullptr || s_submit_lock == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    const TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    const TickType_t started = xTaskGetTickCount();
    bool expected = false;
    if (!s_control_busy.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_submit_lock, timeout_ticks) != pdTRUE) {
        s_control_busy.store(false, std::memory_order_release);
        return ESP_ERR_TIMEOUT;
    }
    const SdCardState card_state = static_cast<SdCardState>(
        s_card_state.load(std::memory_order_acquire));
    const auto observed = sd_storage_service_status();
    if (card_state != SdCardState::ReadyReadWrite ||
        !confirmation.matches(observed.mount_generation, observed.manifest_selected,
                              observed.selected_manifest.dataset_id)) {
        xSemaphoreGive(s_submit_lock);
        s_control_busy.store(false, std::memory_order_release);
        return ESP_ERR_INVALID_STATE;
    }
    close_admission_locked();
    xSemaphoreGive(s_submit_lock);

    while (s_pending_work.load(std::memory_order_acquire) != 0U) {
        if (xTaskGetTickCount() - started >= timeout_ticks) {
            if (xSemaphoreTake(s_submit_lock, portMAX_DELAY) == pdTRUE) {
                open_admission_locked();
                xSemaphoreGive(s_submit_lock);
            }
            s_control_busy.store(false, std::memory_order_release);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (xSemaphoreTake(s_submit_lock, portMAX_DELAY) != pdTRUE) {
        s_control_busy.store(false, std::memory_order_release);
        return ESP_FAIL;
    }
    const uint32_t id = next_command_id();
    Command command = {};
    command.type = CommandType::DeleteApplicationData;
    command.delete_confirmation = confirmation;
    command.id = id;
    if (xQueueSend(s_commands, &command, 0) != pdTRUE) {
        open_admission_locked();
        xSemaphoreGive(s_submit_lock);
        s_control_busy.store(false, std::memory_order_release);
        return ESP_ERR_TIMEOUT;
    }
    xSemaphoreGive(s_submit_lock);

    // Once destructive work is queued, do not return while it is still
    // running. Reopening admission before completion could interleave a new
    // repository request with directory removal.
    while (s_completed_command_id.load(std::memory_order_acquire) != id)
        vTaskDelay(pdMS_TO_TICKS(10));
    const esp_err_t result = static_cast<esp_err_t>(
        s_completed_result.load(std::memory_order_relaxed));
    if (xSemaphoreTake(s_submit_lock, portMAX_DELAY) == pdTRUE) {
        open_admission_locked();
        xSemaphoreGive(s_submit_lock);
    }
    return result;
}

}  // namespace sticky_storage
