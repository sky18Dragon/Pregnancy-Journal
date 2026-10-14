#include "sticky_checkpoint_commit_test.h"

#if defined(STICKY_CHECKPOINT_COMMIT_TEST) && STICKY_CHECKPOINT_COMMIT_TEST

#include <array>
#include <cstring>

#include "app_log.h"
#include "canvas.h"
#include "checkpoint_commit_test_pages.h"
#include "sd_index_power_fixture.h"
#include "sd_journal_payload.h"
#include "sd_storage_service.h"
#include "sticky_display.h"
#include "sticky_touch.h"
#include "storage_crc32.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

using namespace sticky_storage;

constexpr char kTag[] = "checkpoint_commit_test";
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(40);
constexpr uint32_t kBaselineJournalRecords = 36U;
constexpr uint64_t kSnapshotTransaction = 36U;
constexpr uint64_t kCheckpointTransaction = 37U;
constexpr uint32_t kCheckpointSegment = 1U;
constexpr uint32_t kLiveRecords = 45U;

CheckpointCommitTestView s_view = {};
char s_dataset_id[kDatasetIdCharacters + 1U] = {};
uint32_t s_mount_generation = 0U;
SdStorageRequestHandle s_request = {};
SdJournalStoreProbe s_journal = {};
std::array<uint8_t, kSdJournalCheckpointBytes> s_payload = {};
size_t s_payload_size = 0U;
uint32_t s_payload_crc32 = 0U;

void render(Canvas &canvas, bool full = false)
{
    checkpoint_commit_test_page_render(canvas, s_view);
    const esp_err_t result = full ? sticky_display_refresh_monochrome()
                                  : sticky_display_refresh_partial();
    STICKY_LOGI(kTag, "display=refresh phase=%u full=%u result=%s",
                static_cast<unsigned>(s_view.phase), full ? 1U : 0U,
                esp_err_to_name(result));
}

bool snapshot_unchanged()
{
    const SdStorageServiceStatus status = sd_storage_service_status();
    return status.running && status.accepting_journal_requests &&
           status.card_state == SdCardState::ReadyReadWrite &&
           status.mount_generation == s_mount_generation &&
           status.manifest_selected &&
           std::strcmp(status.selected_manifest.dataset_id,
                       s_dataset_id) == 0;
}

void fail(Canvas &canvas, uint8_t status)
{
    s_view.phase = CheckpointCommitTestPhase::Failed;
    s_view.result_status = status;
    render(canvas, true);
}

bool begin_journal_probe(Canvas &canvas)
{
    const SdStorageServiceStatus status = sd_storage_service_status();
    if (!status.running || !status.accepting_journal_requests ||
        status.card_state != SdCardState::ReadyReadWrite ||
        !status.manifest_selected) {
        return false;
    }
    s_mount_generation = status.mount_generation;
    std::memcpy(s_dataset_id, status.selected_manifest.dataset_id,
                sizeof(s_dataset_id));
    if (sd_storage_service_submit_journal_probe(
            s_mount_generation, s_dataset_id, s_request) != ESP_OK) {
        return false;
    }
    s_view.phase = CheckpointCommitTestPhase::ProbingJournal;
    render(canvas, true);
    return true;
}

void poll_journal(Canvas &canvas)
{
    if (s_view.phase != CheckpointCommitTestPhase::ProbingJournal) {
        return;
    }
    SdStorageJournalResult result = {};
    const esp_err_t collected =
        sd_storage_service_collect_journal_result(s_request, result);
    if (collected == ESP_ERR_NOT_FINISHED) {
        return;
    }
    if (collected != ESP_OK ||
        result.outcome.status != SdJournalRequestStatus::Ok) {
        fail(canvas, collected == ESP_OK
                         ? static_cast<uint8_t>(result.outcome.status)
                         : 255U);
        return;
    }
    s_journal = result.outcome.probe;
    s_view.journal_records = s_journal.total_record_count;
    s_view.last_transaction_id = s_journal.active.last.transaction_id;
    const bool baseline =
        s_journal.total_record_count == kBaselineJournalRecords &&
        s_journal.active.last.transaction_id == kSnapshotTransaction;
    const bool committed =
        s_journal.total_record_count == kBaselineJournalRecords + 1U &&
        s_journal.active.last.transaction_id == kCheckpointTransaction &&
        s_journal.active.last.type == SdJournalRecordType::Checkpoint &&
        s_journal.active.last.payload_size == kSdJournalCheckpointBytes;
    if ((!baseline && !committed) ||
        sd_storage_service_submit_index_power_probe(
            s_mount_generation, s_dataset_id, kSnapshotTransaction,
            s_request) != ESP_OK) {
        fail(canvas, 254U);
        return;
    }
    s_view.phase = CheckpointCommitTestPhase::ProbingIndex;
}

bool prepare_payload(const SdIndexFileProbe &probe)
{
    if (probe.status != SdIndexFileProbeStatus::Match ||
        probe.file_bytes != kSdIndexSnapshotHeaderBytes +
                                kIndexPowerFixtureEntries *
                                    kSdIndexSnapshotEntryBytes ||
        probe.file_bytes > UINT32_MAX) {
        return false;
    }
    SdJournalCheckpointPayload checkpoint = {};
    checkpoint.last_transaction_id = kSnapshotTransaction;
    checkpoint.segment_number = kCheckpointSegment;
    checkpoint.live_record_count = kLiveRecords;
    checkpoint.index_bytes = static_cast<uint32_t>(probe.file_bytes);
    checkpoint.index_crc32 = probe.file_crc32;
    std::strcpy(checkpoint.index_file, kIndexPowerFixtureFile);
    s_payload_size = sd_journal_encode_checkpoint_payload(
        checkpoint, s_payload.data(), s_payload.size());
    if (s_payload_size != kSdJournalCheckpointBytes) {
        return false;
    }
    s_payload_crc32 = crc32(s_payload.data(), s_payload_size);
    return true;
}

void poll_index(Canvas &canvas)
{
    if (s_view.phase != CheckpointCommitTestPhase::ProbingIndex) {
        return;
    }
    SdStorageIndexPowerResult result = {};
    const esp_err_t collected =
        sd_storage_service_collect_index_power_result(s_request, result);
    if (collected == ESP_ERR_NOT_FINISHED) {
        return;
    }
    if (collected != ESP_OK || result.service_status != ESP_OK ||
        !prepare_payload(result.probe)) {
        fail(canvas, collected == ESP_OK
                         ? static_cast<uint8_t>(result.write_status)
                         : 255U);
        return;
    }
    s_view.index_bytes = result.probe.file_bytes;
    if (s_journal.total_record_count == kBaselineJournalRecords + 1U) {
        if (s_journal.active.last.payload_crc32 != s_payload_crc32) {
            fail(canvas, 253U);
            return;
        }
        s_view.phase = CheckpointCommitTestPhase::Passed;
        STICKY_LOGI(
            kTag,
            "checkpoint=verified journal=%u frame_tx=%llu snapshot_tx=%llu index_bytes=%llu payload_crc=%08X",
            static_cast<unsigned>(s_view.journal_records),
            static_cast<unsigned long long>(s_view.last_transaction_id),
            static_cast<unsigned long long>(kSnapshotTransaction),
            static_cast<unsigned long long>(s_view.index_bytes),
            static_cast<unsigned>(s_payload_crc32));
    } else {
        s_view.phase = CheckpointCommitTestPhase::Ready;
    }
    render(canvas, true);
}

void submit_checkpoint(Canvas &canvas)
{
    if (!snapshot_unchanged() || s_payload_size == 0U) {
        fail(canvas, 252U);
        return;
    }
    // The display and MicroSD share SPI2. Finish the e-paper update before
    // queueing the append so the storage worker cannot overlap panel I/O.
    s_view.phase = CheckpointCommitTestPhase::Appending;
    render(canvas);
    const esp_err_t submitted = sd_storage_service_submit_journal_append(
        s_mount_generation, s_dataset_id, SdJournalRecordType::Checkpoint,
        kCheckpointTransaction, s_payload.data(), s_payload_size, s_request);
    if (submitted != ESP_OK) {
        fail(canvas, 251U);
        return;
    }
    STICKY_LOGI(kTag,
                "checkpoint=submit frame_tx=%llu snapshot_tx=%llu payload_crc=%08X",
                static_cast<unsigned long long>(kCheckpointTransaction),
                static_cast<unsigned long long>(kSnapshotTransaction),
                static_cast<unsigned>(s_payload_crc32));
}

void poll_append(Canvas &canvas)
{
    if (s_view.phase != CheckpointCommitTestPhase::Appending) {
        return;
    }
    SdStorageJournalResult result = {};
    const esp_err_t collected =
        sd_storage_service_collect_journal_result(s_request, result);
    if (collected == ESP_ERR_NOT_FINISHED) {
        return;
    }
    const bool passed =
        collected == ESP_OK &&
        result.outcome.status == SdJournalRequestStatus::Ok &&
        result.outcome.probe.total_record_count ==
            kBaselineJournalRecords + 1U &&
        result.outcome.probe.active.last.type ==
            SdJournalRecordType::Checkpoint &&
        result.outcome.probe.active.last.transaction_id ==
            kCheckpointTransaction &&
        result.outcome.probe.active.last.payload_crc32 == s_payload_crc32;
    if (!passed) {
        fail(canvas, collected == ESP_OK
                         ? static_cast<uint8_t>(result.outcome.status)
                         : 255U);
        return;
    }
    s_view.journal_records = result.outcome.probe.total_record_count;
    s_view.last_transaction_id =
        result.outcome.probe.active.last.transaction_id;
    s_view.phase = CheckpointCommitTestPhase::Passed;
    STICKY_LOGI(kTag,
                "checkpoint=complete journal=%u last_tx=%llu changed=%u pass=1",
                static_cast<unsigned>(s_view.journal_records),
                static_cast<unsigned long long>(
                    s_view.last_transaction_id),
                result.outcome.changed ? 1U : 0U);
    render(canvas, true);
}

}  // namespace

[[noreturn]] void sticky_checkpoint_commit_test_run(Canvas &canvas)
{
    app_log_register_tag(kTag);
    sticky_display_set_battery_overlay_enabled(false);
    s_view = {};
    s_view.target_transaction_id = kCheckpointTransaction;
    render(canvas, true);

    while (true) {
        if (s_view.phase == CheckpointCommitTestPhase::Waiting) {
            begin_journal_probe(canvas);
        }
        poll_journal(canvas);
        poll_index(canvas);
        poll_append(canvas);

        StickyTouchPress press = {};
        if (sticky_touch_take_press(press)) {
            int x = 0;
            int y = 0;
            canvas.physical_to_logical(press.x, press.y, x, y);
            const CheckpointCommitTestAction action =
                checkpoint_commit_test_page_action_at(
                    s_view, canvas.width(), canvas.height(), x, y);
            if (action == CheckpointCommitTestAction::Continue &&
                snapshot_unchanged()) {
                s_view.phase = CheckpointCommitTestPhase::Confirm;
                render(canvas);
            } else if (action == CheckpointCommitTestAction::Cancel) {
                s_view.phase = CheckpointCommitTestPhase::Ready;
                render(canvas);
            } else if (action == CheckpointCommitTestAction::Commit) {
                submit_checkpoint(canvas);
            }
        }
        vTaskDelay(kPollInterval);
    }
}

#endif
