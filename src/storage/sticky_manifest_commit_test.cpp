#include "sticky_manifest_commit_test.h"

#if defined(STICKY_MANIFEST_COMMIT_TEST) && STICKY_MANIFEST_COMMIT_TEST

#include <cstring>

#include "app_log.h"
#include "canvas.h"
#include "manifest_commit_test_pages.h"
#include "sd_manifest.h"
#include "sd_storage_service.h"
#include "sticky_display.h"
#include "sticky_touch.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

using namespace sticky_storage;

constexpr char kTag[] = "manifest_commit_test";
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(40);

ManifestCommitTestView s_view = {};
SdManifest s_baseline = {};
uint32_t s_mount_generation = 0U;
SdStorageRequestHandle s_request = {};

char slot_name(SdManifestSlot slot)
{
    return slot == SdManifestSlot::A ? 'A' : 'B';
}

void render(Canvas &canvas, bool full = false)
{
    manifest_commit_test_page_render(canvas, s_view);
    const esp_err_t result = full ? sticky_display_refresh_monochrome()
                                  : sticky_display_refresh_partial();
    STICKY_LOGI(kTag, "display=refresh phase=%u full=%u result=%s",
                static_cast<unsigned>(s_view.phase), full ? 1U : 0U,
                esp_err_to_name(result));
}

bool load_ready_snapshot()
{
    const SdStorageServiceStatus status = sd_storage_service_status();
    if (!status.running || !status.accepting_manifest_requests ||
        status.card_state != SdCardState::ReadyReadWrite ||
        !status.manifest_known || !status.manifest_selected ||
        (status.manifest_state != SdManifestProbeState::Ready &&
         status.manifest_state != SdManifestProbeState::Degraded)) {
        return false;
    }

    s_baseline = status.selected_manifest;
    s_mount_generation = status.mount_generation;
    s_view.current_generation = s_baseline.commit_generation;
    s_view.current_slot = slot_name(status.selected_slot);
    SdManifest next = {};
    if (!sd_manifest_make_next(s_baseline, next)) {
        return false;
    }
    s_view.target_generation = next.commit_generation;
    s_view.target_slot = slot_name(
        sd_manifest_slot_for_generation(next.commit_generation));
    return true;
}

bool snapshot_unchanged()
{
    const SdStorageServiceStatus status = sd_storage_service_status();
    return status.running && status.accepting_manifest_requests &&
           status.card_state == SdCardState::ReadyReadWrite &&
           status.mount_generation == s_mount_generation &&
           status.manifest_selected &&
           status.selected_manifest.schema_version ==
               s_baseline.schema_version &&
           status.selected_manifest.commit_generation ==
               s_baseline.commit_generation &&
           std::strcmp(status.selected_manifest.dataset_id,
                       s_baseline.dataset_id) == 0;
}

void submit_next(Canvas &canvas)
{
    if (!snapshot_unchanged()) {
        s_view.phase = ManifestCommitTestPhase::Failed;
        s_view.result_status = static_cast<uint8_t>(
            SdManifestRequestStatus::StaleGeneration);
        render(canvas);
        return;
    }

    SdManifest next = {};
    if (!sd_manifest_make_next(s_baseline, next)) {
        s_view.phase = ManifestCommitTestPhase::Failed;
        s_view.result_status = static_cast<uint8_t>(
            SdManifestRequestStatus::GenerationOverflow);
        render(canvas);
        return;
    }

    const esp_err_t result = sd_storage_service_submit_manifest_commit(
        s_mount_generation, next, s_request);
    if (result != ESP_OK) {
        s_view.phase = ManifestCommitTestPhase::Failed;
        s_view.result_status = 255U;
        STICKY_LOGE(kTag, "submit=result_%s", esp_err_to_name(result));
    } else {
        s_view.phase = ManifestCommitTestPhase::Pending;
        STICKY_LOGI(kTag,
                    "submit=request_%u mount_generation=%u commit=%llu slot=%c",
                    static_cast<unsigned>(s_request.id),
                    static_cast<unsigned>(s_mount_generation),
                    static_cast<unsigned long long>(next.commit_generation),
                    s_view.target_slot);
    }
    render(canvas);
}

void poll_result(Canvas &canvas)
{
    if (s_view.phase != ManifestCommitTestPhase::Pending) {
        return;
    }
    SdStorageManifestResult result = {};
    const esp_err_t collected = sd_storage_service_collect_manifest_result(
        s_request, result);
    if (collected == ESP_ERR_NOT_FINISHED) {
        return;
    }

    const bool passed =
        collected == ESP_OK &&
        result.outcome.status == SdManifestRequestStatus::Ok &&
        result.outcome.changed &&
        result.outcome.manifest.commit_generation == s_view.target_generation &&
        slot_name(result.outcome.slot) == s_view.target_slot;
    s_view.phase = passed ? ManifestCommitTestPhase::Passed
                          : ManifestCommitTestPhase::Failed;
    s_view.result_status =
        collected == ESP_OK
            ? static_cast<uint8_t>(result.outcome.status)
            : 255U;
    STICKY_LOGI(
        kTag,
        "complete=request_%u collect=%s status=%u changed=%u commit=%llu slot=%c pass=%u",
        static_cast<unsigned>(s_request.id), esp_err_to_name(collected),
        static_cast<unsigned>(s_view.result_status),
        collected == ESP_OK && result.outcome.changed ? 1U : 0U,
        collected == ESP_OK
            ? static_cast<unsigned long long>(
                  result.outcome.manifest.commit_generation)
            : 0ULL,
        collected == ESP_OK ? slot_name(result.outcome.slot) : '-',
        passed ? 1U : 0U);
    render(canvas, true);
}

}  // namespace

[[noreturn]] void sticky_manifest_commit_test_run(Canvas &canvas)
{
    app_log_register_tag(kTag);
    sticky_display_set_battery_overlay_enabled(false);
    s_view = {};
    render(canvas, true);

    while (true) {
        if (s_view.phase == ManifestCommitTestPhase::Waiting &&
            load_ready_snapshot()) {
            s_view.phase = ManifestCommitTestPhase::Ready;
            STICKY_LOGI(kTag,
                        "phase=ready mount_generation=%u commit=%llu slot=%c",
                        static_cast<unsigned>(s_mount_generation),
                        static_cast<unsigned long long>(
                            s_view.current_generation),
                        s_view.current_slot);
            render(canvas, true);
        }

        StickyTouchPress press = {};
        if (sticky_touch_take_press(press)) {
            int x = 0;
            int y = 0;
            canvas.physical_to_logical(press.x, press.y, x, y);
            const ManifestCommitTestAction action =
                manifest_commit_test_page_action_at(
                    s_view, canvas.width(), canvas.height(), x, y);
            if (action == ManifestCommitTestAction::Continue &&
                snapshot_unchanged()) {
                s_view.phase = ManifestCommitTestPhase::Confirm;
                render(canvas);
            } else if (action == ManifestCommitTestAction::Cancel) {
                s_view.phase = ManifestCommitTestPhase::Ready;
                render(canvas);
            } else if (action == ManifestCommitTestAction::WriteNext) {
                submit_next(canvas);
            }
        }

        poll_result(canvas);
        vTaskDelay(kPollInterval);
    }
}

#endif
