#include "sticky_object_power_test.h"

#if defined(STICKY_OBJECT_POWER_TEST) && STICKY_OBJECT_POWER_TEST

#include <cstring>

#include "app_log.h"
#include "canvas.h"
#include "object_power_test_pages.h"
#include "sd_object_power_fixture.h"
#include "sd_record_object_file.h"
#include "sd_storage_service.h"
#include "sticky_buzzer.h"
#include "sticky_display.h"
#include "sticky_touch.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

using namespace sticky_storage;

constexpr char kTag[] = "object_power_test";
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(40);
#if defined(STICKY_OBJECT_PARTIAL_TEST) && STICKY_OBJECT_PARTIAL_TEST
constexpr bool kPartialMode = true;
#else
constexpr bool kPartialMode = false;
#endif

ObjectPowerTestView s_view = {};
char s_dataset_id[kDatasetIdCharacters + 1U] = {};
uint32_t s_mount_generation = 0U;
SdStorageRequestHandle s_request = {};

void render(Canvas &canvas, bool full = false)
{
    object_power_test_page_render(canvas, s_view);
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
    s_view.phase = ObjectPowerTestPhase::ProbingJournal;
    render(canvas, true);
    return true;
}

void poll_journal(Canvas &canvas)
{
    if (s_view.phase != ObjectPowerTestPhase::ProbingJournal) {
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
        s_view.phase = ObjectPowerTestPhase::Failed;
        render(canvas, true);
        return;
    }
    s_view.journal_records = result.outcome.probe.total_record_count;
    s_view.last_transaction_id = s_view.journal_records == 0U
                                     ? 0U
                                     : result.outcome.probe.active.last
                                           .transaction_id;
    if (sd_storage_service_submit_object_power_probe(
            s_mount_generation, s_dataset_id, s_request) != ESP_OK) {
        s_view.phase = ObjectPowerTestPhase::Failed;
        render(canvas, true);
        return;
    }
    s_view.phase = ObjectPowerTestPhase::ProbingObject;
}

void poll_object(Canvas &canvas)
{
    if (s_view.phase != ObjectPowerTestPhase::ProbingObject &&
        s_view.phase != ObjectPowerTestPhase::Writing) {
        return;
    }
    const bool was_writing =
        s_view.phase == ObjectPowerTestPhase::Writing;
    SdStorageObjectPowerResult result = {};
    const esp_err_t collected =
        sd_storage_service_collect_object_power_result(s_request, result);
    if (collected == ESP_ERR_NOT_FINISHED) {
        return;
    }
    const bool expected_partial_write =
        was_writing && kPartialMode &&
        result.write_status == SdRecordObjectWriteStatus::ShortWrite &&
        result.changed &&
        result.probe.status == SdRecordObjectProbeStatus::Mismatch &&
        result.probe.file_bytes == kObjectPowerPartialFixtureBytes &&
        result.probe.file_crc32 ==
            sd_object_power_partial_fixture_crc32();
    if (collected != ESP_OK || result.service_status != ESP_OK ||
        (was_writing && !expected_partial_write &&
         result.write_status != SdRecordObjectWriteStatus::Ok)) {
        s_view.phase = ObjectPowerTestPhase::Failed;
        render(canvas, true);
        return;
    }
    s_view.object_status = static_cast<uint8_t>(result.probe.status);
    s_view.object_bytes = result.probe.file_bytes;
    s_view.object_crc32 = result.probe.file_crc32;
    if (expected_partial_write) {
        s_view.phase = ObjectPowerTestPhase::AwaitingReset;
        STICKY_LOGI(kTag,
                    "object=partial_synced bytes=%llu crc=%08lX reset=required",
                    static_cast<unsigned long long>(result.probe.file_bytes),
                    static_cast<unsigned long>(result.probe.file_crc32));
        sticky_buzzer_play_pattern(StickyBuzzerPattern::Wake, 0U);
        render(canvas, true);
        return;
    }
    if (result.probe.status == SdRecordObjectProbeStatus::Missing) {
        s_view.phase = ObjectPowerTestPhase::Ready;
    } else if (result.probe.status == SdRecordObjectProbeStatus::Match) {
        s_view.phase = kPartialMode ? ObjectPowerTestPhase::Failed
                                   : ObjectPowerTestPhase::CompleteOrphan;
    } else if (result.probe.status == SdRecordObjectProbeStatus::Mismatch) {
        const bool expected_partial =
            !kPartialMode ||
            (result.probe.file_bytes == kObjectPowerPartialFixtureBytes &&
             result.probe.file_crc32 ==
                 sd_object_power_partial_fixture_crc32());
        s_view.phase = expected_partial
                           ? ObjectPowerTestPhase::PartialOrphan
                           : ObjectPowerTestPhase::Failed;
    } else {
        s_view.phase = ObjectPowerTestPhase::Failed;
    }
    STICKY_LOGI(kTag,
                "object=complete status=%u bytes=%llu journal=%u last_tx=%llu",
                static_cast<unsigned>(result.probe.status),
                static_cast<unsigned long long>(result.probe.file_bytes),
                static_cast<unsigned>(s_view.journal_records),
                static_cast<unsigned long long>(
                    s_view.last_transaction_id));
    render(canvas, true);
}

void start_write(Canvas &canvas)
{
    if (!snapshot_unchanged()) {
        s_view.phase = ObjectPowerTestPhase::Failed;
        render(canvas, true);
        return;
    }
    s_view.phase = ObjectPowerTestPhase::Writing;
    render(canvas, true);
    sticky_buzzer_play_pattern(StickyBuzzerPattern::Wake, 0U);
    const esp_err_t submitted =
        sd_storage_service_submit_object_power_write(
            s_mount_generation, s_dataset_id, s_request);
    STICKY_LOGI(kTag, "object=submit result=%s",
                esp_err_to_name(submitted));
    if (submitted != ESP_OK) {
        s_view.phase = ObjectPowerTestPhase::Failed;
        render(canvas, true);
    }
}

}  // namespace

[[noreturn]] void sticky_object_power_test_run(Canvas &canvas)
{
    app_log_register_tag(kTag);
    sticky_display_set_battery_overlay_enabled(false);
    s_view = {};
    s_view.partial_mode = kPartialMode;
    render(canvas, true);
    while (true) {
        if (s_view.phase == ObjectPowerTestPhase::Waiting) {
            begin_journal_probe(canvas);
        }
        poll_journal(canvas);
        poll_object(canvas);

        StickyTouchPress press = {};
        if (sticky_touch_take_press(press)) {
            int x = 0;
            int y = 0;
            canvas.physical_to_logical(press.x, press.y, x, y);
            const ObjectPowerTestAction action =
                object_power_test_page_action_at(
                    s_view, canvas.width(), canvas.height(), x, y);
            if (action == ObjectPowerTestAction::Continue &&
                snapshot_unchanged()) {
                s_view.phase = ObjectPowerTestPhase::Confirm;
                render(canvas);
            } else if (action == ObjectPowerTestAction::Cancel) {
                s_view.phase = ObjectPowerTestPhase::Ready;
                render(canvas);
            } else if (action == ObjectPowerTestAction::Start) {
                start_write(canvas);
            }
        }
        vTaskDelay(kPollInterval);
    }
}

#endif
