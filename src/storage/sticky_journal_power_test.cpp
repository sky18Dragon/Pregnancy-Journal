#include "sticky_journal_power_test.h"

#if defined(STICKY_JOURNAL_POWER_TEST) && STICKY_JOURNAL_POWER_TEST

#include <array>
#include <cstdio>
#include <cstring>

#include "app_log.h"
#include "canvas.h"
#include "journal_power_test_pages.h"
#include "sd_journal_payload.h"
#include "sd_journal_power_fixture.h"
#include "sd_storage_service.h"
#include "sticky_buzzer.h"
#include "sticky_display.h"
#include "sticky_touch.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

using namespace sticky_storage;

constexpr char kTag[] = "journal_power_test";
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(40);
#if defined(STICKY_JOURNAL_PARTIAL_TEST) && STICKY_JOURNAL_PARTIAL_TEST
constexpr bool kPartialMode = true;
#else
constexpr bool kPartialMode = false;
#endif
// The final physical tail test deliberately uses the largest canonical PJTX
// payload (four mutations, 344 bytes including the journal header). Seven
// frames keep the known one-record-per-frame fixture history plus this run
// within the 64-slot recovery index while crossing several 512-byte media
// sectors. Requests are submitted back-to-back so the reset window is spent in
// real journal I/O instead of an artificial idle delay.
constexpr uint32_t kMaximumWrites = 7U;
constexpr uint32_t kMutationsPerWrite = 4U;
// A probe exposes frame count rather than recovered live-record count. Assume
// the worst case (four new records per prior frame) so a repeated stress run
// can never exceed the 64-slot recovery index.
constexpr uint32_t kMaximumBaselineRecords = 9U;

JournalPowerTestView s_view = {};
char s_dataset_id[kDatasetIdCharacters + 1U] = {};
uint32_t s_mount_generation = 0U;
SdStorageRequestHandle s_request = {};

void render(Canvas &canvas, bool full = false)
{
    journal_power_test_page_render(canvas, s_view);
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

bool begin_probe(Canvas &canvas)
{
    const SdStorageServiceStatus status = sd_storage_service_status();
    if (!status.running || !status.accepting_journal_requests ||
        status.card_state != SdCardState::ReadyReadWrite ||
        !status.manifest_selected ||
        (status.manifest_state != SdManifestProbeState::Ready &&
         status.manifest_state != SdManifestProbeState::Degraded)) {
        return false;
    }
    s_mount_generation = status.mount_generation;
    std::memcpy(s_dataset_id, status.selected_manifest.dataset_id,
                sizeof(s_dataset_id));
    const esp_err_t submitted = sd_storage_service_submit_journal_probe(
        s_mount_generation, s_dataset_id, s_request);
    if (submitted != ESP_OK) {
        return false;
    }
    s_view.phase = JournalPowerTestPhase::Probing;
    STICKY_LOGI(kTag, "probe=submit request=%u generation=%u",
                static_cast<unsigned>(s_request.id),
                static_cast<unsigned>(s_mount_generation));
    render(canvas, true);
    return true;
}

void copy_probe(const SdJournalStoreProbe &probe)
{
    s_view.probe_status = static_cast<uint8_t>(probe.status);
    s_view.record_count = probe.total_record_count;
    s_view.file_bytes = probe.status == SdJournalStoreStatus::TailTruncated
                            ? probe.active.file_bytes
                            : probe.total_bytes;
    s_view.valid_bytes = probe.status == SdJournalStoreStatus::TailTruncated
                             ? probe.active.valid_bytes
                             : probe.total_bytes;
    s_view.last_transaction_id =
        probe.total_record_count == 0U ? 0U
                                      : probe.active.last.transaction_id;
    s_view.target_transaction_id = s_view.last_transaction_id + 1U;
}

void fail(Canvas &canvas, uint8_t status)
{
    s_view.result_status = status;
    s_view.phase = JournalPowerTestPhase::Failed;
    render(canvas, true);
}

void poll_probe(Canvas &canvas)
{
    if (s_view.phase != JournalPowerTestPhase::Probing) {
        return;
    }
    SdStorageJournalResult result = {};
    const esp_err_t collected =
        sd_storage_service_collect_journal_result(s_request, result);
    if (collected == ESP_ERR_NOT_FINISHED) {
        return;
    }
    if (collected != ESP_OK) {
        fail(canvas, 255U);
        return;
    }
    copy_probe(result.outcome.probe);
    STICKY_LOGI(
        kTag,
        "probe=complete status=%u store=%u records=%u bytes=%llu valid=%llu last_tx=%llu",
        static_cast<unsigned>(result.outcome.status),
        static_cast<unsigned>(result.outcome.probe.status),
        static_cast<unsigned>(s_view.record_count),
        static_cast<unsigned long long>(s_view.file_bytes),
        static_cast<unsigned long long>(s_view.valid_bytes),
        static_cast<unsigned long long>(s_view.last_transaction_id));
    if (result.outcome.status == SdJournalRequestStatus::TailTruncated) {
        const bool expected_tail =
            !kPartialMode ||
            (s_view.record_count == kJournalPartialBaselineRecords &&
             s_view.last_transaction_id ==
                 kJournalPartialBaselineTransaction &&
             s_view.file_bytes == kJournalPartialExpectedFileBytes &&
             s_view.valid_bytes == kJournalPartialBaselineBytes);
        s_view.phase = expected_tail ? JournalPowerTestPhase::TailRecovered
                                     : JournalPowerTestPhase::Failed;
    } else if (result.outcome.status == SdJournalRequestStatus::Ok &&
               s_view.last_transaction_id != UINT64_MAX &&
               ((!kPartialMode &&
                 s_view.record_count <= kMaximumBaselineRecords) ||
                (kPartialMode &&
                 s_view.record_count == kJournalPartialBaselineRecords &&
                 s_view.last_transaction_id ==
                     kJournalPartialBaselineTransaction &&
                 s_view.file_bytes == kJournalPartialBaselineBytes &&
                 s_view.valid_bytes == kJournalPartialBaselineBytes))) {
        s_view.phase = JournalPowerTestPhase::Ready;
    } else {
        s_view.result_status =
            static_cast<uint8_t>(result.outcome.status);
        s_view.phase = JournalPowerTestPhase::Failed;
    }
    render(canvas, true);
}

void submit_fixture(Canvas &canvas)
{
    if (!snapshot_unchanged()) {
        fail(canvas,
             static_cast<uint8_t>(SdJournalRequestStatus::StaleGeneration));
        return;
    }
    const esp_err_t submitted =
        sd_storage_service_submit_journal_power_fixture(
            s_mount_generation, s_dataset_id, s_request);
    if (submitted != ESP_OK) {
        fail(canvas, 255U);
        return;
    }
    s_view.phase = JournalPowerTestPhase::Preparing;
    STICKY_LOGI(kTag, "fixture=submit request=%u",
                static_cast<unsigned>(s_request.id));
    render(canvas);
}

void poll_fixture(Canvas &canvas)
{
    if (s_view.phase != JournalPowerTestPhase::Preparing) {
        return;
    }
    SdStorageJournalResult result = {};
    const esp_err_t collected =
        sd_storage_service_collect_journal_result(s_request, result);
    if (collected == ESP_ERR_NOT_FINISHED) {
        return;
    }
    const bool expected_partial =
        kPartialMode && collected == ESP_OK && result.outcome.changed &&
        result.outcome.status == SdJournalRequestStatus::TailTruncated;
    if (collected != ESP_OK ||
        (!expected_partial &&
         result.outcome.status != SdJournalRequestStatus::Ok)) {
        fail(canvas, collected == ESP_OK
                         ? static_cast<uint8_t>(result.outcome.status)
                         : 255U);
        return;
    }
    copy_probe(result.outcome.probe);
    STICKY_LOGI(kTag, "fixture=ready changed=%u status=%u bytes=%llu valid=%llu",
                result.outcome.changed ? 1U : 0U,
                static_cast<unsigned>(result.outcome.status),
                static_cast<unsigned long long>(s_view.file_bytes),
                static_cast<unsigned long long>(s_view.valid_bytes));
    if (expected_partial &&
        s_view.record_count == kJournalPartialBaselineRecords &&
        s_view.last_transaction_id == kJournalPartialBaselineTransaction &&
        s_view.file_bytes == kJournalPartialExpectedFileBytes &&
        s_view.valid_bytes == kJournalPartialBaselineBytes) {
        s_view.phase = JournalPowerTestPhase::AwaitingReset;
        sticky_buzzer_play_pattern(StickyBuzzerPattern::Wake, 0U);
    } else if (kPartialMode) {
        fail(canvas, 255U);
        return;
    } else {
        s_view.phase = JournalPowerTestPhase::Armed;
    }
    render(canvas, true);
}

bool submit_next_append()
{
    if (!snapshot_unchanged()) {
        return false;
    }
    const uint64_t transaction_id = s_view.target_transaction_id;
    SdJournalTransactionPayload transaction = {};
    transaction.mutation_count = kMutationsPerWrite;
    for (uint32_t index = 0U; index < kMutationsPerWrite; ++index) {
        char record_id[kRepositoryRecordIdCharacters + 1U] = {};
        std::snprintf(record_id, sizeof(record_id), "%016llx%016llx",
                      static_cast<unsigned long long>(0x8000000000000000ULL +
                                                      index),
                      static_cast<unsigned long long>(transaction_id));
        SdJournalMutationReference &mutation = transaction.mutations[index];
        if (!repository_key_make(RepositoryKind::Note, record_id,
                                 mutation.key)) {
            return false;
        }
        mutation.type = RepositoryMutationType::Upsert;
        mutation.expected_revision = 0U;
        mutation.committed_revision = 1U;
        mutation.object_bytes = kJournalPowerFixtureObjectBytes;
        mutation.object_crc32 = sd_journal_power_fixture_object_crc32();
        std::strcpy(mutation.object_file, kJournalPowerFixtureObjectFile);
    }

    std::array<uint8_t, kSdJournalMaxPayloadBytes> payload = {};
    const size_t payload_size = sd_journal_encode_transaction_payload(
        transaction, payload.data(), payload.size());
    if (payload_size == 0U) {
        return false;
    }
    const esp_err_t submitted = sd_storage_service_submit_journal_append(
        s_mount_generation, s_dataset_id, SdJournalRecordType::Commit,
        transaction_id, payload.data(), payload_size, s_request);
    if (submitted != ESP_OK) {
        return false;
    }
    STICKY_LOGI(kTag, "append=submit request=%u tx=%llu cycle=%u",
                static_cast<unsigned>(s_request.id),
                static_cast<unsigned long long>(transaction_id),
                static_cast<unsigned>(s_view.completed_writes + 1U));
    return true;
}

void start_loop(Canvas &canvas)
{
    s_view.phase = JournalPowerTestPhase::Running;
    s_view.completed_writes = 0U;
    render(canvas, true);
    const esp_err_t cue =
        sticky_buzzer_play_pattern(StickyBuzzerPattern::Wake, 0U);
    STICKY_LOGI(kTag, "write_cue=start result=%s", esp_err_to_name(cue));
    if (!submit_next_append()) {
        fail(canvas, 255U);
    }
}

void poll_append(Canvas &canvas)
{
    if (s_view.phase != JournalPowerTestPhase::Running) {
        return;
    }
    SdStorageJournalResult result = {};
    const esp_err_t collected =
        sd_storage_service_collect_journal_result(s_request, result);
    if (collected == ESP_ERR_NOT_FINISHED) {
        return;
    }
    if (collected != ESP_OK ||
        result.outcome.status != SdJournalRequestStatus::Ok ||
        !result.outcome.changed) {
        fail(canvas, collected == ESP_OK
                         ? static_cast<uint8_t>(result.outcome.status)
                         : 255U);
        return;
    }
    ++s_view.completed_writes;
    copy_probe(result.outcome.probe);
    STICKY_LOGI(kTag,
                "append=complete tx=%llu cycle=%u records=%u result=ok",
                static_cast<unsigned long long>(
                    s_view.last_transaction_id),
                static_cast<unsigned>(s_view.completed_writes),
                static_cast<unsigned>(s_view.record_count));
    if (s_view.completed_writes >= kMaximumWrites) {
        s_view.phase = JournalPowerTestPhase::Completed;
        render(canvas, true);
        return;
    }
    if (!submit_next_append()) {
        fail(canvas, 255U);
    }
}

}  // namespace

[[noreturn]] void sticky_journal_power_test_run(Canvas &canvas)
{
    app_log_register_tag(kTag);
    sticky_display_set_battery_overlay_enabled(false);
    s_view = {};
    s_view.partial_mode = kPartialMode;
    render(canvas, true);

    while (true) {
        if (s_view.phase == JournalPowerTestPhase::Waiting) {
            begin_probe(canvas);
        }
        poll_probe(canvas);
        poll_fixture(canvas);
        poll_append(canvas);

        StickyTouchPress press = {};
        if (sticky_touch_take_press(press)) {
            int x = 0;
            int y = 0;
            canvas.physical_to_logical(press.x, press.y, x, y);
            const JournalPowerTestAction action =
                journal_power_test_page_action_at(
                    s_view, canvas.width(), canvas.height(), x, y);
            if (action == JournalPowerTestAction::Continue &&
                snapshot_unchanged()) {
                s_view.phase = JournalPowerTestPhase::Confirm;
                render(canvas);
            } else if (action == JournalPowerTestAction::Cancel) {
                s_view.phase = JournalPowerTestPhase::Ready;
                render(canvas);
            } else if (action == JournalPowerTestAction::Prepare) {
                submit_fixture(canvas);
            } else if (action == JournalPowerTestAction::Start) {
                start_loop(canvas);
            }
        }
        vTaskDelay(kPollInterval);
    }
}

#endif
