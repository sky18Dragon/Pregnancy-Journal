#include "sticky_storage_status_test.h"

#if defined(STICKY_STORAGE_STATUS_TEST) && STICKY_STORAGE_STATUS_TEST

#include <cstdio>
#include <cstring>

#include "app_log.h"
#include "canvas.h"
#include "prototype_ui.h"
#include "sd_storage_service.h"
#include "sticky_display.h"
#include "ui_tokens.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
using namespace sticky_storage;

constexpr char kTag[] = "storage_status_test";

const char *test_subtitle()
{
#if defined(STICKY_SD_INJECT_READ_ONLY) && STICKY_SD_INJECT_READ_ONLY
    return "INJECTED READ-ONLY STATE";
#elif defined(STICKY_SD_INJECT_FULL) && STICKY_SD_INJECT_FULL
    return "INJECTED ZERO-FREE STATE";
#else
    return "READ-ONLY S1 CARD INSPECTION";
#endif
}

const char *card_name(SdCardState state)
{
    switch (state) {
    case SdCardState::Absent: return "ABSENT";
    case SdCardState::Mounting: return "MOUNTING";
    case SdCardState::ReadyReadWrite: return "READ WRITE";
    case SdCardState::ReadyReadOnly: return "READ ONLY";
    case SdCardState::Full: return "FULL";
    case SdCardState::Fault: return "FAULT";
    case SdCardState::Ejecting: return "EJECTING";
    case SdCardState::Unmounted: return "UNMOUNTED";
    }
    return "UNKNOWN";
}

const char *manifest_name(SdManifestProbeState state)
{
    switch (state) {
    case SdManifestProbeState::NotInitialized: return "NOT INITIALIZED";
    case SdManifestProbeState::Ready: return "READY";
    case SdManifestProbeState::Degraded: return "DEGRADED";
    case SdManifestProbeState::Corrupt: return "CORRUPT";
    case SdManifestProbeState::IoError: return "IO ERROR";
    }
    return "UNKNOWN";
}

const char *write_check_name(SdStoragePolicyStatus status)
{
    switch (status) {
    case SdStoragePolicyStatus::Ok: return "OK";
    case SdStoragePolicyStatus::InvalidArgument: return "INVALID ARGUMENT";
    case SdStoragePolicyStatus::DatasetMismatch: return "DATASET MISMATCH";
    case SdStoragePolicyStatus::StaleGeneration: return "STALE GENERATION";
    case SdStoragePolicyStatus::PathOutsideApplicationRoot: return "BAD ROOT";
    case SdStoragePolicyStatus::InvalidPhysicalPath: return "BAD PATH";
    case SdStoragePolicyStatus::CardUnavailable: return "CARD UNAVAILABLE";
    case SdStoragePolicyStatus::CardReadOnly: return "CARD READ ONLY";
    case SdStoragePolicyStatus::CapacityUnavailable: return "NO CAPACITY";
    case SdStoragePolicyStatus::UsageUnavailable: return "NO USAGE";
    case SdStoragePolicyStatus::NoSpace: return "NO SPACE";
    case SdStoragePolicyStatus::AudioQuotaReached: return "AUDIO QUOTA";
    }
    return "UNKNOWN";
}

const char *recovery_name(SdRecoveryLoadStatus status)
{
    switch (status) {
    case SdRecoveryLoadStatus::NotAttempted: return "NOT ATTEMPTED";
    case SdRecoveryLoadStatus::Empty: return "EMPTY READY";
    case SdRecoveryLoadStatus::ReadyJournal: return "JOURNAL READY";
    case SdRecoveryLoadStatus::ReadyCheckpoint: return "CHECKPOINT READY";
    case SdRecoveryLoadStatus::TailTruncated: return "TAIL DEGRADED";
    case SdRecoveryLoadStatus::JournalInvalid: return "JOURNAL INVALID";
    case SdRecoveryLoadStatus::PayloadInvalid: return "PAYLOAD INVALID";
    case SdRecoveryLoadStatus::CheckpointInvalid: return "CHECKPOINT INVALID";
    case SdRecoveryLoadStatus::IndexMissing: return "INDEX MISSING";
    case SdRecoveryLoadStatus::IndexMismatch: return "INDEX MISMATCH";
    case SdRecoveryLoadStatus::ObjectMissing: return "OBJECT MISSING";
    case SdRecoveryLoadStatus::ObjectMismatch: return "OBJECT MISMATCH";
    case SdRecoveryLoadStatus::IoError: return "IO ERROR";
    case SdRecoveryLoadStatus::InvalidArgument: return "INVALID ARGUMENT";
    }
    return "UNKNOWN";
}

void render(Canvas &canvas, const SdStorageServiceStatus &status)
{
    prototype_ui::begin_page(canvas);
    prototype_ui::draw_header(canvas, "STORAGE STATUS TEST", test_subtitle());
    char line[128] = {};
    std::snprintf(line, sizeof(line), "CARD: %s   MOUNT: %u",
                  card_name(status.card_state),
                  static_cast<unsigned>(status.mount_generation));
    canvas.draw_text_pixels(24, 116, line, ui_tokens::kSectionHeading,
                            GrayLevel::Black, FontWeight::Medium);
    std::snprintf(line, sizeof(line), "MANIFEST: %s",
                  status.manifest_known
                      ? manifest_name(status.manifest_state)
                      : "PROBING");
    canvas.draw_text_pixels(24, 154, line, ui_tokens::kSectionHeading,
                            GrayLevel::Black, FontWeight::Medium);
    std::snprintf(line, sizeof(line), "RECOVERY: %s",
                  status.recovery_known
                      ? recovery_name(status.recovery_status)
                      : "PROBING");
    canvas.draw_text_pixels(24, 192, line, ui_tokens::kSectionHeading,
                            GrayLevel::Black, FontWeight::Medium);
    std::snprintf(
        line, sizeof(line), "JOURNAL: %u   LAST TX: %llu   SNAPSHOT: %llu",
        static_cast<unsigned>(status.recovery_journal_records),
        static_cast<unsigned long long>(
            status.recovery_last_transaction_id),
        static_cast<unsigned long long>(
            status.recovery_snapshot_transaction_id));
    canvas.draw_text_pixels(24, 230, line, ui_tokens::kSectionHeading,
                            GrayLevel::Black, FontWeight::Medium);
    std::snprintf(line, sizeof(line),
                  "LIVE: %u   VERIFIED OBJECTS: %u   VIEW: %s",
                  static_cast<unsigned>(status.recovery_live_records),
                  static_cast<unsigned>(status.recovery_verified_objects),
                  status.recovery_view_ready ? "READY" : "BLOCKED");
    canvas.draw_text_pixels(24, 268, line, ui_tokens::kSectionHeading,
                            GrayLevel::Black, FontWeight::Medium);
    if (status.capacity_known) {
        std::snprintf(
            line, sizeof(line), "CAPACITY: %llu MiB   FREE: %llu MiB",
            static_cast<unsigned long long>(status.total_bytes / 1048576ULL),
            static_cast<unsigned long long>(status.free_bytes / 1048576ULL));
    } else {
        std::snprintf(line, sizeof(line), "CAPACITY: UNKNOWN");
    }
    canvas.draw_text_pixels(24, 344, line, ui_tokens::kBody,
                            GrayLevel::Black, FontWeight::Medium);
    if (status.manifest_selected) {
        std::snprintf(
            line, sizeof(line), "DATA: %.8s...  GEN: %llu  SLOT: %c",
            status.selected_manifest.dataset_id,
            static_cast<unsigned long long>(
                status.selected_manifest.commit_generation),
            status.selected_slot == SdManifestSlot::A ? 'A' : 'B');
    } else {
        std::snprintf(line, sizeof(line), "DATA: NONE");
    }
    canvas.draw_text_pixels(24, 306, line, ui_tokens::kSectionHeading,
                            GrayLevel::Black, FontWeight::Medium);
    std::snprintf(line, sizeof(line), "AUDIO: %u / %llu B   WRITE: %s",
                  static_cast<unsigned>(status.audio_items),
                  static_cast<unsigned long long>(status.audio_bytes),
                  write_check_name(sd_storage_service_check_write(
                      SdStorageWriteClass::Business, 1U)));
    canvas.draw_text_pixels(24, 380, line, ui_tokens::kBody,
                            GrayLevel::Black, FontWeight::Regular);
    canvas.draw_text_pixels(24, 424,
                            "READ-ONLY RECOVERY CHECK; NO FILES ARE MODIFIED.",
                            ui_tokens::kBody, GrayLevel::Black,
                            FontWeight::Regular);
    sticky_display_refresh_monochrome();
}

bool changed(const SdStorageServiceStatus &a,
             const SdStorageServiceStatus &b)
{
    return a.card_state != b.card_state ||
           a.mount_generation != b.mount_generation ||
           a.manifest_known != b.manifest_known ||
           a.manifest_state != b.manifest_state ||
           a.manifest_selected != b.manifest_selected ||
           a.selected_manifest.commit_generation !=
               b.selected_manifest.commit_generation ||
           std::strcmp(a.selected_manifest.dataset_id,
                       b.selected_manifest.dataset_id) != 0 ||
           a.selected_slot != b.selected_slot ||
           a.capacity_known != b.capacity_known ||
           a.total_bytes != b.total_bytes || a.free_bytes != b.free_bytes ||
           a.audio_usage_known != b.audio_usage_known ||
           a.audio_items != b.audio_items || a.audio_bytes != b.audio_bytes ||
           a.recovery_known != b.recovery_known ||
           a.recovery_status != b.recovery_status ||
           a.recovery_view_ready != b.recovery_view_ready ||
           a.recovery_checkpoint_used != b.recovery_checkpoint_used ||
           a.recovery_journal_records != b.recovery_journal_records ||
           a.recovery_last_transaction_id !=
               b.recovery_last_transaction_id ||
           a.recovery_snapshot_transaction_id !=
               b.recovery_snapshot_transaction_id ||
           a.recovery_live_records != b.recovery_live_records ||
           a.recovery_verified_objects != b.recovery_verified_objects;
}

void log_status(const SdStorageServiceStatus &status)
{
    STICKY_LOGI(
        kTag,
        "card=%s mount=%u manifest=%s dataset=%s generation=%llu slot=%c recovery=%s view=%u checkpoint=%u journal=%u last_tx=%llu snapshot_tx=%llu live=%u objects=%u capacity_known=%u total=%llu free=%llu audio_known=%u audio_items=%u audio_bytes=%llu write_check=%s",
        card_name(status.card_state),
        static_cast<unsigned>(status.mount_generation),
        status.manifest_known ? manifest_name(status.manifest_state)
                              : "PROBING",
        status.manifest_selected ? status.selected_manifest.dataset_id : "none",
        static_cast<unsigned long long>(
            status.manifest_selected
                ? status.selected_manifest.commit_generation
                : 0U),
        status.manifest_selected
            ? (status.selected_slot == SdManifestSlot::A ? 'A' : 'B')
            : '-',
        status.recovery_known ? recovery_name(status.recovery_status)
                              : "PROBING",
        status.recovery_view_ready ? 1U : 0U,
        status.recovery_checkpoint_used ? 1U : 0U,
        static_cast<unsigned>(status.recovery_journal_records),
        static_cast<unsigned long long>(
            status.recovery_last_transaction_id),
        static_cast<unsigned long long>(
            status.recovery_snapshot_transaction_id),
        static_cast<unsigned>(status.recovery_live_records),
        static_cast<unsigned>(status.recovery_verified_objects),
        status.capacity_known ? 1U : 0U,
        static_cast<unsigned long long>(status.total_bytes),
        static_cast<unsigned long long>(status.free_bytes),
        status.audio_usage_known ? 1U : 0U,
        static_cast<unsigned>(status.audio_items),
        static_cast<unsigned long long>(status.audio_bytes),
        write_check_name(sd_storage_service_check_write(
            SdStorageWriteClass::Business, 1U)));
}

}  // namespace

[[noreturn]] void sticky_storage_status_test_run(Canvas &canvas)
{
    app_log_register_tag(kTag);
    sticky_display_set_battery_overlay_enabled(false);
    SdStorageServiceStatus previous = {};
    bool rendered = false;
    unsigned heartbeat_ticks = 0;
    while (true) {
        const SdStorageServiceStatus status = sd_storage_service_status();
        const bool status_changed = !rendered || changed(status, previous);
        if (status_changed || heartbeat_ticks >= 25U) {
            log_status(status);
            heartbeat_ticks = 0;
        }
        if (status_changed) {
            render(canvas, status);
            previous = status;
            rendered = true;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
        ++heartbeat_ticks;
    }
}

#endif
