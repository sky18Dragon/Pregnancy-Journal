#include "sticky_index_power_test.h"

#if defined(STICKY_INDEX_POWER_TEST) && STICKY_INDEX_POWER_TEST

#include <cstring>

#include "app_log.h"
#include "canvas.h"
#include "index_power_test_pages.h"
#include "sd_index_snapshot.h"
#include "sd_storage_service.h"
#include "sticky_buzzer.h"
#include "sticky_display.h"
#include "sticky_touch.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

using namespace sticky_storage;

constexpr char kTag[] = "index_power_test";
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(40);
#if defined(STICKY_INDEX_PARTIAL_TEST) && STICKY_INDEX_PARTIAL_TEST
constexpr bool kPartialMode = true;
constexpr uint32_t kExpectedJournalRecords = 37U;
constexpr uint64_t kExpectedLastTransaction = 37U;
#else
constexpr bool kPartialMode = false;
constexpr uint32_t kExpectedJournalRecords = 36U;
constexpr uint64_t kExpectedLastTransaction = 36U;
#endif

IndexPowerTestView s_view = {};
char s_dataset_id[kDatasetIdCharacters + 1U] = {};
uint32_t s_mount_generation = 0U;
SdStorageRequestHandle s_request = {};

void render(Canvas &canvas, bool full = false)
{
    index_power_test_page_render(canvas, s_view);
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
    s_view.phase = IndexPowerTestPhase::ProbingJournal;
    render(canvas, true);
    return true;
}

void fail(Canvas &canvas)
{
    s_view.phase = IndexPowerTestPhase::Failed;
    render(canvas, true);
}

void poll_journal(Canvas &canvas)
{
    if (s_view.phase != IndexPowerTestPhase::ProbingJournal) {
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
        fail(canvas);
        return;
    }
    s_view.journal_records = result.outcome.probe.total_record_count;
    s_view.last_transaction_id = s_view.journal_records == 0U
                                     ? 0U
                                     : result.outcome.probe.active.last
                                           .transaction_id;
    if (s_view.journal_records != kExpectedJournalRecords ||
        s_view.last_transaction_id != kExpectedLastTransaction ||
        sd_storage_service_submit_index_power_probe(
            s_mount_generation, s_dataset_id,
            s_view.last_transaction_id, s_request) != ESP_OK) {
        fail(canvas);
        return;
    }
    s_view.phase = IndexPowerTestPhase::ProbingIndex;
}

void poll_index(Canvas &canvas)
{
    if (s_view.phase != IndexPowerTestPhase::ProbingIndex &&
        s_view.phase != IndexPowerTestPhase::Writing) {
        return;
    }
    const bool was_writing = s_view.phase == IndexPowerTestPhase::Writing;
    SdStorageIndexPowerResult result = {};
    const esp_err_t collected =
        sd_storage_service_collect_index_power_result(s_request, result);
    if (collected == ESP_ERR_NOT_FINISHED) {
        return;
    }
    const bool expected_partial_write =
        was_writing && kPartialMode &&
        result.write_status == SdIndexWriteStatus::ShortWrite &&
        result.changed && result.partial_prefix_match;
    if (collected != ESP_OK || result.service_status != ESP_OK ||
        (was_writing && !expected_partial_write &&
         result.write_status != SdIndexWriteStatus::Ok)) {
        fail(canvas);
        return;
    }
    s_view.index_bytes = result.probe.file_bytes;
    if (expected_partial_write) {
        s_view.phase = IndexPowerTestPhase::AwaitingReset;
        STICKY_LOGI(kTag,
                    "index=partial_synced bytes=%llu crc=%08lX reset=required",
                    static_cast<unsigned long long>(result.probe.file_bytes),
                    static_cast<unsigned long>(result.probe.file_crc32));
        sticky_buzzer_play_pattern(StickyBuzzerPattern::Wake, 0U);
        render(canvas, true);
        return;
    }
    if (result.probe.status == SdIndexFileProbeStatus::Missing) {
        s_view.phase = IndexPowerTestPhase::Ready;
    } else if (result.probe.status == SdIndexFileProbeStatus::Match) {
        s_view.phase = kPartialMode ? IndexPowerTestPhase::Failed
                                   : IndexPowerTestPhase::CompleteOrphan;
    } else if (result.probe.status == SdIndexFileProbeStatus::Mismatch) {
        s_view.phase = !kPartialMode || result.partial_prefix_match
                           ? IndexPowerTestPhase::PartialOrphan
                           : IndexPowerTestPhase::Failed;
    } else {
        s_view.phase = IndexPowerTestPhase::Failed;
    }
    STICKY_LOGI(kTag,
                "index=complete status=%u bytes=%llu journal=%u last_tx=%llu",
                static_cast<unsigned>(result.probe.status),
                static_cast<unsigned long long>(result.probe.file_bytes),
                static_cast<unsigned>(s_view.journal_records),
                static_cast<unsigned long long>(s_view.last_transaction_id));
    render(canvas, true);
}

void start_write(Canvas &canvas)
{
    if (!snapshot_unchanged()) {
        fail(canvas);
        return;
    }
    s_view.phase = IndexPowerTestPhase::Writing;
    render(canvas, true);
    sticky_buzzer_play_pattern(StickyBuzzerPattern::Wake, 0U);
    const esp_err_t submitted = sd_storage_service_submit_index_power_write(
        s_mount_generation, s_dataset_id, s_view.last_transaction_id,
        s_request);
    STICKY_LOGI(kTag, "index=submit result=%s", esp_err_to_name(submitted));
    if (submitted != ESP_OK) {
        fail(canvas);
    }
}

}  // namespace

[[noreturn]] void sticky_index_power_test_run(Canvas &canvas)
{
    app_log_register_tag(kTag);
    sticky_display_set_battery_overlay_enabled(false);
    s_view = {};
    s_view.partial_mode = kPartialMode;
    render(canvas, true);
    while (true) {
        if (s_view.phase == IndexPowerTestPhase::Waiting) {
            begin_journal_probe(canvas);
        }
        poll_journal(canvas);
        poll_index(canvas);

        StickyTouchPress press = {};
        if (sticky_touch_take_press(press)) {
            int x = 0;
            int y = 0;
            canvas.physical_to_logical(press.x, press.y, x, y);
            const IndexPowerTestAction action =
                index_power_test_page_action_at(
                    s_view, canvas.width(), canvas.height(), x, y);
            if (action == IndexPowerTestAction::Continue &&
                snapshot_unchanged()) {
                s_view.phase = IndexPowerTestPhase::Confirm;
                render(canvas);
            } else if (action == IndexPowerTestAction::Cancel) {
                s_view.phase = IndexPowerTestPhase::Ready;
                render(canvas);
            } else if (action == IndexPowerTestAction::Start) {
                start_write(canvas);
            }
        }
        vTaskDelay(kPollInterval);
    }
}

#endif
