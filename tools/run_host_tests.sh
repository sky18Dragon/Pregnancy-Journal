#!/usr/bin/env bash
set -euo pipefail

readonly PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly BUILD_DIR="${TMPDIR:-/tmp}/sticky_core_host_tests"
readonly CXX_BIN="${CXX:-g++}"
readonly COMMON_FLAGS=(-std=c++17 -Wall -Wextra -Werror)
mkdir -p "${BUILD_DIR}"
cd "${PROJECT_ROOT}"
passed_count=0

compile_and_run() {
    local name="$1"
    shift
    echo "[host-test] ${name}"
    "${CXX_BIN}" "${COMMON_FLAGS[@]}" "$@" -o "${BUILD_DIR}/${name}"
    "${BUILD_DIR}/${name}"
    passed_count=$((passed_count + 1))
}

compile_and_run sticky_app_lifecycle_test -pthread -Itest/lifecycle_support -Itest/support -Isrc/app test/sticky_app_lifecycle_test.cpp src/app/sticky_app_lifecycle.cpp

compile_and_run device_preferences_sync_test -Itest/support -Isrc/app -Isrc/storage -Isrc/ui test/device_preferences_sync_test.cpp

compile_and_run board_sd_card_test -Itest/sd_support -Itest/support -Isrc/board \
    test/board_sd_card_test.cpp src/board/board_sd_card.cpp
compile_and_run fake_storage_repository_test -Isrc/storage \
    test/fake_storage_repository_test.cpp \
    src/storage/fake_storage_repository.cpp \
    src/storage/storage_repository.cpp src/storage/sd_manifest.cpp
compile_and_run domain_repository_records_test -Itest/support -Isrc/app \
    -Isrc/apps/pregnancy -Isrc/pregnancy/models \
    -Isrc/pregnancy/services -Isrc/storage -Isrc/ui \
    test/domain_repository_records_test.cpp \
    src/pregnancy/services/domain_repository_records.cpp \
    src/pregnancy/services/kick_service.cpp \
    src/pregnancy/services/weight_service.cpp \
    src/apps/pregnancy/pregnancy_state.cpp \
    src/storage/storage_repository.cpp src/storage/sd_manifest.cpp \
    src/app/sticky_app_id.cpp
compile_and_run sd_repository_adapter_test -Itest/support -Isrc/app \
    -Isrc/apps/pregnancy -Isrc/pregnancy/models \
    -Isrc/pregnancy/services -Isrc/storage -Isrc/ui \
    test/sd_repository_adapter_test.cpp \
    src/storage/sd_repository_adapter.cpp \
    src/storage/legacy_data_migration.cpp \
    src/pregnancy/services/pregnancy_repository_store.cpp \
    src/pregnancy/services/domain_repository_stores.cpp \
    src/pregnancy/services/domain_repository_records.cpp \
    src/pregnancy/services/kick_service.cpp \
    src/pregnancy/services/weight_service.cpp \
    src/apps/pregnancy/pregnancy_state.cpp \
    src/storage/storage_repository.cpp src/storage/sd_manifest.cpp \
    src/app/sticky_app_id.cpp
compile_and_run legacy_nvs_source_test -Itest/support -Isrc/app \
    -Isrc/apps/pregnancy -Isrc/pregnancy/models -Isrc/pregnancy/services \
    -Isrc/storage -Isrc/ui test/legacy_nvs_source_test.cpp \
    src/storage/legacy_nvs_source.cpp src/storage/persistent_state.cpp \
    src/pregnancy/services/kick_storage.cpp src/pregnancy/services/kick_service.cpp \
    src/pregnancy/services/domain_repository_records.cpp \
    src/pregnancy/services/weight_service.cpp src/pregnancy/services/weight_storage.cpp \
    src/apps/pregnancy/pregnancy_state.cpp src/apps/pregnancy/pregnancy_storage_record.cpp \
    src/storage/storage_repository.cpp \
    src/storage/sd_manifest.cpp src/app/sticky_app_id.cpp
compile_and_run sd_storage_policy_test -Isrc/storage \
    test/sd_storage_policy_test.cpp src/storage/sd_storage_policy.cpp \
    src/storage/sd_manifest.cpp
compile_and_run sd_application_data_delete_test -Isrc/storage \
    test/sd_application_data_delete_test.cpp \
    src/storage/sd_application_data_delete.cpp
compile_and_run sd_storage_work_gate_test -Isrc/storage \
    test/sd_storage_work_gate_test.cpp \
    src/storage/sd_storage_work_gate.cpp \
    src/storage/sd_storage_policy.cpp src/storage/sd_manifest.cpp
compile_and_run sd_repository_recovery_test -Isrc/storage \
    test/sd_repository_recovery_test.cpp \
    src/storage/sd_repository_recovery.cpp \
    src/storage/sd_journal_payload.cpp src/storage/storage_repository.cpp \
    src/storage/sd_manifest.cpp
compile_and_run domain_repository_integration_test -Isrc/storage -Itest/support -Isrc/app -Isrc/apps/pregnancy -Isrc/pregnancy/models -Isrc/pregnancy/services -Isrc/ui \
    test/domain_repository_integration_test.cpp \
    src/storage/sd_manifest_request.cpp src/storage/sd_manifest_writer.cpp \
    src/storage/sd_manifest_reader.cpp src/storage/sd_recovery_loader.cpp \
    src/storage/sd_index_snapshot.cpp \
    src/storage/sd_repository_request.cpp \
    src/storage/sd_repository_recovery.cpp \
    src/storage/sd_record_object_file.cpp \
    src/storage/sd_journal_store.cpp src/storage/sd_journal_segments.cpp \
    src/storage/sd_journal_file.cpp src/storage/sd_journal.cpp \
    src/storage/sd_journal_payload.cpp src/storage/sd_storage_policy.cpp \
    src/storage/storage_repository.cpp src/storage/sd_manifest.cpp \
    src/pregnancy/services/domain_repository_records.cpp \
    src/pregnancy/services/kick_service.cpp \
    src/pregnancy/services/weight_service.cpp \
    src/apps/pregnancy/pregnancy_state.cpp \
    src/app/sticky_app_id.cpp
compile_and_run sd_repository_request_test -Isrc/storage \
    test/sd_repository_request_test.cpp \
    src/storage/sd_manifest_request.cpp src/storage/sd_manifest_writer.cpp \
    src/storage/sd_manifest_reader.cpp src/storage/sd_recovery_loader.cpp \
    src/storage/sd_index_snapshot.cpp \
    src/storage/sd_repository_request.cpp \
    src/storage/sd_repository_recovery.cpp \
    src/storage/sd_record_object_file.cpp \
    src/storage/sd_journal_store.cpp src/storage/sd_journal_segments.cpp \
    src/storage/sd_journal_file.cpp src/storage/sd_journal.cpp \
    src/storage/sd_journal_payload.cpp src/storage/sd_storage_policy.cpp \
    src/storage/storage_repository.cpp src/storage/sd_manifest.cpp
compile_and_run sd_repository_completion_test -Isrc/storage \
    test/sd_repository_completion_test.cpp \
    src/storage/sd_repository_completion.cpp \
    src/storage/sd_repository_request.cpp \
    src/storage/sd_repository_recovery.cpp \
    src/storage/sd_record_object_file.cpp \
    src/storage/sd_journal_store.cpp src/storage/sd_journal_segments.cpp \
    src/storage/sd_journal_file.cpp src/storage/sd_journal.cpp \
    src/storage/sd_journal_payload.cpp src/storage/sd_storage_policy.cpp \
    src/storage/storage_repository.cpp src/storage/sd_manifest.cpp
compile_and_run sd_recovery_loader_test -Isrc/storage \
    test/sd_recovery_loader_test.cpp \
    src/storage/sd_recovery_loader.cpp \
    src/storage/sd_repository_recovery.cpp \
    src/storage/sd_record_object_file.cpp \
    src/storage/sd_index_snapshot.cpp \
    src/storage/sd_journal_store.cpp \
    src/storage/sd_journal_segments.cpp \
    src/storage/sd_journal_file.cpp src/storage/sd_journal.cpp \
    src/storage/sd_journal_payload.cpp \
    src/storage/sd_storage_policy.cpp \
    src/storage/storage_repository.cpp src/storage/sd_manifest.cpp
compile_and_run sd_journal_test -Isrc/storage \
    test/sd_journal_test.cpp src/storage/sd_journal.cpp \
    src/storage/sd_manifest.cpp
compile_and_run sd_journal_payload_test -Isrc/storage \
    test/sd_journal_payload_test.cpp src/storage/sd_journal_payload.cpp \
    src/storage/storage_repository.cpp src/storage/sd_manifest.cpp
compile_and_run sd_journal_power_fixture_test -Isrc/storage \
    test/sd_journal_power_fixture_test.cpp \
    src/storage/sd_journal_power_fixture.cpp \
    src/storage/sd_journal_payload.cpp src/storage/sd_journal.cpp \
    src/storage/storage_repository.cpp src/storage/sd_manifest.cpp
compile_and_run sd_object_power_fixture_test -Isrc/storage \
    test/sd_object_power_fixture_test.cpp \
    src/storage/sd_object_power_fixture.cpp
compile_and_run sd_record_object_file_test -Isrc/storage \
    test/sd_record_object_file_test.cpp \
    src/storage/sd_record_object_file.cpp
compile_and_run sd_index_snapshot_test -Isrc/storage \
    test/sd_index_snapshot_test.cpp src/storage/sd_index_snapshot.cpp \
    src/storage/sd_index_power_fixture.cpp \
    src/storage/sd_journal_power_fixture.cpp \
    src/storage/sd_journal_payload.cpp src/storage/sd_journal.cpp \
    src/storage/storage_repository.cpp src/storage/sd_manifest.cpp
compile_and_run sd_journal_segments_test -Isrc/storage \
    test/sd_journal_segments_test.cpp src/storage/sd_journal_segments.cpp
compile_and_run sd_journal_store_test -Isrc/storage \
    test/sd_journal_store_test.cpp src/storage/sd_journal_store.cpp \
    src/storage/sd_journal_segments.cpp src/storage/sd_journal_file.cpp \
    src/storage/sd_journal.cpp src/storage/sd_storage_policy.cpp \
    src/storage/sd_manifest.cpp
compile_and_run sd_journal_file_test -Isrc/storage \
    test/sd_journal_file_test.cpp src/storage/sd_journal_file.cpp \
    src/storage/sd_journal.cpp src/storage/sd_manifest.cpp
compile_and_run sd_journal_request_test -Isrc/storage \
    test/sd_journal_request_test.cpp src/storage/sd_journal_request.cpp \
    src/storage/sd_journal_payload.cpp src/storage/storage_repository.cpp \
    src/storage/sd_journal_store.cpp src/storage/sd_journal_segments.cpp \
    src/storage/sd_journal_file.cpp src/storage/sd_journal.cpp \
    src/storage/sd_storage_policy.cpp \
    src/storage/sd_manifest_reader.cpp src/storage/sd_manifest.cpp
compile_and_run sd_journal_completion_test -Isrc/storage \
    test/sd_journal_completion_test.cpp \
    src/storage/sd_journal_completion.cpp \
    src/storage/sd_journal_request.cpp src/storage/sd_journal_store.cpp \
    src/storage/sd_journal_payload.cpp src/storage/storage_repository.cpp \
    src/storage/sd_journal_segments.cpp src/storage/sd_journal_file.cpp \
    src/storage/sd_journal.cpp src/storage/sd_manifest_reader.cpp \
    src/storage/sd_storage_policy.cpp src/storage/sd_manifest.cpp
compile_and_run sd_manifest_test -Isrc/storage \
    test/sd_manifest_test.cpp src/storage/sd_manifest.cpp
compile_and_run sd_manifest_completion_test -Isrc/storage \
    test/sd_manifest_completion_test.cpp \
    src/storage/sd_manifest_completion.cpp
compile_and_run manifest_commit_test_pages_test -Isrc/storage -Isrc/ui \
    -Isrc/ui/assets \
    test/manifest_commit_test_pages_test.cpp \
    src/storage/manifest_commit_test_pages.cpp src/ui/canvas.cpp \
    src/ui/font.cpp src/ui/prototype_ui.cpp src/ui/text_layout.cpp \
    src/ui/ui_language.cpp src/ui/assets/chinese_font_assets.cpp
compile_and_run journal_power_test_pages_test -Isrc/storage -Isrc/ui \
    -Isrc/ui/assets \
    test/journal_power_test_pages_test.cpp \
    src/storage/journal_power_test_pages.cpp src/ui/canvas.cpp \
    src/ui/font.cpp src/ui/prototype_ui.cpp src/ui/text_layout.cpp \
    src/ui/ui_language.cpp src/ui/assets/chinese_font_assets.cpp
compile_and_run object_power_test_pages_test -Isrc/storage -Isrc/ui \
    -Isrc/ui/assets \
    test/object_power_test_pages_test.cpp \
    src/storage/object_power_test_pages.cpp src/ui/canvas.cpp \
    src/ui/font.cpp src/ui/prototype_ui.cpp src/ui/text_layout.cpp \
    src/ui/ui_language.cpp src/ui/assets/chinese_font_assets.cpp
compile_and_run index_power_test_pages_test -Isrc/storage -Isrc/ui \
    -Isrc/ui/assets \
    test/index_power_test_pages_test.cpp \
    src/storage/index_power_test_pages.cpp src/ui/canvas.cpp \
    src/ui/font.cpp src/ui/prototype_ui.cpp src/ui/text_layout.cpp \
    src/ui/ui_language.cpp src/ui/assets/chinese_font_assets.cpp
compile_and_run checkpoint_commit_test_pages_test -Isrc/storage -Isrc/ui \
    -Isrc/ui/assets \
    test/checkpoint_commit_test_pages_test.cpp \
    src/storage/checkpoint_commit_test_pages.cpp src/ui/canvas.cpp \
    src/ui/font.cpp src/ui/prototype_ui.cpp src/ui/text_layout.cpp \
    src/ui/ui_language.cpp src/ui/assets/chinese_font_assets.cpp
compile_and_run sd_manifest_reader_test -Isrc/storage \
    test/sd_manifest_reader_test.cpp src/storage/sd_manifest_reader.cpp \
    src/storage/sd_manifest.cpp
compile_and_run sd_manifest_request_test -Isrc/storage \
    test/sd_manifest_request_test.cpp src/storage/sd_manifest_request.cpp \
    src/storage/sd_manifest_writer.cpp src/storage/sd_manifest_reader.cpp \
    src/storage/sd_manifest.cpp
compile_and_run sd_manifest_writer_test -Isrc/storage \
    test/sd_manifest_writer_test.cpp src/storage/sd_manifest_writer.cpp \
    src/storage/sd_manifest_reader.cpp src/storage/sd_manifest.cpp
compile_and_run sd_card_state_test -Isrc/storage \
    test/sd_card_state_test.cpp src/storage/sd_card_state.cpp
compile_and_run sticky_sdcard_test -Itest/support -Isrc/board -Isrc/devices \
    -Isrc/storage \
    test/sticky_sdcard_test.cpp src/devices/sticky_sdcard.cpp \
    src/storage/sd_card_state.cpp
compile_and_run app_manager_test -Itest/support -Isrc/app \
    test/app_manager_test.cpp src/app/app_manager.cpp
compile_and_run app_registry_test -Itest/support -Isrc/app -Isrc/ui \
    -Isrc/apps/pregnancy \
    -Isrc/apps/weight -Isrc/apps/kick -Isrc/apps/settings \
    test/app_registry_test.cpp src/app/app_registry.cpp
compile_and_run refresh_policy_test -Isrc/ui test/refresh_policy_test.cpp
compile_and_run press_feedback_test -Isrc/ui -Isrc/ui/assets \
    test/press_feedback_test.cpp src/ui/canvas.cpp src/ui/font.cpp \
    src/ui/prototype_ui.cpp src/ui/text_layout.cpp \
    src/ui/ui_language.cpp src/ui/assets/chinese_font_assets.cpp
compile_and_run digit_fields_test -Isrc/ui -Isrc/ui/assets \
    test/digit_fields_test.cpp src/ui/digit_fields.cpp src/ui/canvas.cpp \
    src/ui/font.cpp src/ui/ui_language.cpp src/ui/prototype_ui.cpp \
    src/ui/text_layout.cpp src/ui/assets/chinese_font_assets.cpp
compile_and_run pregnancy_flow_test -Isrc/apps/pregnancy -Isrc/pregnancy/content -Isrc/app \
    -Isrc/ui -Isrc/ui/assets \
    test/pregnancy_flow_test.cpp src/apps/pregnancy/pregnancy_flow.cpp \
    src/apps/pregnancy/pregnancy_pages.cpp src/ui/status_glyphs.cpp src/apps/pregnancy/pregnancy_state.cpp \
    src/ui/digit_fields.cpp src/ui/canvas.cpp src/ui/font.cpp src/ui/prototype_ui.cpp \
    src/ui/text_layout.cpp src/ui/ui_language.cpp \
    src/ui/assets/chinese_font_assets.cpp src/ui/assets/pixel_asset.cpp \
    src/ui/assets/app_launcher_assets.cpp \
    src/ui/assets/pregnancy_plate_assets.cpp \
    src/pregnancy/content/content_service.cpp
compile_and_run app_launcher_test -Isrc/app -Isrc/ui -Isrc/ui/assets \
    test/app_launcher_test.cpp src/ui/app_pages.cpp src/ui/canvas.cpp \
    src/ui/font.cpp src/ui/prototype_ui.cpp src/ui/text_layout.cpp src/ui/ui_language.cpp \
    src/ui/assets/chinese_font_assets.cpp
compile_and_run font_test_screen_test -Isrc/ui -Isrc/ui/assets \
    test/font_test_screen_test.cpp src/ui/font_test_screen.cpp \
    src/ui/canvas.cpp src/ui/font.cpp src/ui/ui_language.cpp \
    src/ui/assets/chinese_font_assets.cpp
compile_and_run battery_status_overlay_theme_test -Isrc/ui -Isrc/ui/assets \
    test/battery_status_overlay_theme_test.cpp \
    src/ui/battery_status_overlay_theme.cpp src/ui/canvas.cpp \
    src/ui/font.cpp src/ui/ui_language.cpp \
    src/ui/assets/chinese_font_assets.cpp
compile_and_run status_glyphs_test -Isrc/ui -Isrc/ui/assets \
    test/status_glyphs_test.cpp src/ui/status_glyphs.cpp src/ui/prototype_ui.cpp \
    src/ui/battery_status_overlay_theme.cpp src/ui/canvas.cpp src/ui/font.cpp \
    src/ui/text_layout.cpp src/ui/ui_language.cpp \
    src/ui/assets/chinese_font_assets.cpp src/ui/assets/pixel_asset.cpp
compile_and_run persistent_state_test -Isrc/app -Isrc/storage -Isrc/ui \
    test/persistent_state_test.cpp src/storage/persistent_state.cpp \
    src/app/sticky_app_id.cpp
compile_and_run sticky_sleep_schedule_test -Isrc/app -Isrc/system \
    test/sticky_sleep_schedule_test.cpp src/app/sticky_sleep_schedule.cpp \
    src/system/scheduler.cpp
compile_and_run scheduler_test -Isrc/system \
    test/scheduler_test.cpp src/system/scheduler.cpp
compile_and_run repository_status_page_test -Itest/support -Isrc/storage -Isrc/ui -Isrc/ui/assets \
    test/repository_status_page_test.cpp src/ui/repository_status_page.cpp \
    src/ui/canvas.cpp src/ui/font.cpp src/ui/prototype_ui.cpp \
    src/ui/text_layout.cpp src/ui/ui_language.cpp src/ui/assets/chinese_font_assets.cpp
compile_and_run settings_pages_test -Isrc/apps/settings -Isrc/apps/pregnancy \
    -Isrc/ui -Isrc/ui/assets \
    test/settings_pages_test.cpp src/apps/settings/settings_pages.cpp src/ui/digit_fields.cpp \
    src/ui/canvas.cpp src/ui/font.cpp src/ui/prototype_ui.cpp \
    src/ui/text_layout.cpp src/ui/ui_language.cpp \
    src/ui/assets/chinese_font_assets.cpp
compile_and_run settings_flow_test -Isrc/apps/settings -Isrc/apps/pregnancy \
    -Isrc/ui -Isrc/ui/assets \
    test/settings_flow_test.cpp src/apps/settings/settings_flow.cpp \
    src/apps/settings/settings_pages.cpp src/apps/pregnancy/pregnancy_state.cpp \
    src/ui/digit_fields.cpp src/ui/canvas.cpp src/ui/font.cpp src/ui/prototype_ui.cpp \
    src/ui/text_layout.cpp src/ui/ui_language.cpp \
    src/ui/assets/chinese_font_assets.cpp
compile_and_run pregnancy_state_test -Isrc/apps/pregnancy \
    test/pregnancy_state_test.cpp src/apps/pregnancy/pregnancy_state.cpp
compile_and_run pregnancy_storage_record_test -Isrc/apps/pregnancy \
    test/pregnancy_storage_record_test.cpp \
    src/apps/pregnancy/pregnancy_storage_record.cpp \
    src/apps/pregnancy/pregnancy_state.cpp
compile_and_run pregnancy_pages_test \
    -Isrc/apps/pregnancy -Isrc/pregnancy/content -Isrc/app -Isrc/ui -Isrc/ui/assets \
    test/pregnancy_pages_test.cpp src/apps/pregnancy/pregnancy_pages.cpp src/ui/status_glyphs.cpp src/ui/digit_fields.cpp \
    src/apps/pregnancy/pregnancy_state.cpp src/ui/canvas.cpp src/ui/font.cpp \
    src/ui/prototype_ui.cpp src/ui/text_layout.cpp src/ui/ui_language.cpp \
    src/ui/assets/chinese_font_assets.cpp src/ui/assets/pixel_asset.cpp \
    src/ui/assets/app_launcher_assets.cpp \
    src/ui/assets/pregnancy_plate_assets.cpp \
    src/pregnancy/content/content_service.cpp
compile_and_run content_service_test -Isrc/pregnancy/content \
    test/content_service_test.cpp src/pregnancy/content/content_service.cpp
compile_and_run weight_service_test -Isrc/apps/pregnancy \
    -Isrc/pregnancy/models -Isrc/pregnancy/services \
    test/weight_service_test.cpp src/pregnancy/services/weight_service.cpp \
    src/apps/pregnancy/pregnancy_state.cpp
compile_and_run kick_counter_test -Isrc/apps/pregnancy -Isrc/pregnancy/models \
    -Isrc/pregnancy/services test/kick_counter_test.cpp \
    src/pregnancy/services/kick_counter.cpp src/apps/pregnancy/pregnancy_state.cpp
compile_and_run kick_service_test -Isrc/apps/pregnancy \
    -Isrc/pregnancy/models -Isrc/pregnancy/services \
    test/kick_service_test.cpp src/pregnancy/services/kick_service.cpp \
    src/apps/pregnancy/pregnancy_state.cpp
compile_and_run kick_comparison_test -Isrc/apps/pregnancy \
    -Isrc/pregnancy/models -Isrc/pregnancy/services \
    test/kick_comparison_test.cpp src/pregnancy/services/kick_comparison.cpp \
    src/pregnancy/services/kick_service.cpp src/apps/pregnancy/pregnancy_state.cpp
compile_and_run weight_pages_test -Isrc/apps/weight -Isrc/apps/pregnancy \
    -Isrc/pregnancy/models -Isrc/pregnancy/services -Isrc/ui -Isrc/ui/assets \
    test/weight_pages_test.cpp src/apps/weight/weight_pages.cpp \
    src/pregnancy/services/weight_service.cpp src/apps/pregnancy/pregnancy_state.cpp \
    src/ui/canvas.cpp src/ui/font.cpp src/ui/prototype_ui.cpp src/ui/ui_language.cpp \
    src/ui/text_layout.cpp src/ui/assets/chinese_font_assets.cpp
compile_and_run weight_flow_test -Isrc/apps/weight -Isrc/apps/pregnancy \
    -Isrc/pregnancy/models -Isrc/pregnancy/services -Isrc/ui -Isrc/ui/assets \
    test/weight_flow_test.cpp src/apps/weight/weight_flow.cpp \
    src/apps/weight/weight_pages.cpp src/pregnancy/services/weight_service.cpp \
    src/apps/pregnancy/pregnancy_state.cpp \
    src/ui/canvas.cpp src/ui/font.cpp src/ui/prototype_ui.cpp src/ui/ui_language.cpp \
    src/ui/text_layout.cpp src/ui/assets/chinese_font_assets.cpp
compile_and_run kick_pages_test -Isrc/apps/kick -Isrc/apps/pregnancy \
    -Isrc/pregnancy/services \
    -Isrc/pregnancy/models -Isrc/ui -Isrc/ui/assets \
    test/kick_pages_test.cpp src/apps/kick/kick_pages.cpp \
    src/ui/canvas.cpp src/ui/font.cpp src/ui/prototype_ui.cpp \
    src/ui/text_layout.cpp src/ui/ui_language.cpp \
    src/ui/assets/chinese_font_assets.cpp
compile_and_run kick_flow_test -Isrc/apps/kick -Isrc/apps/pregnancy \
    -Isrc/pregnancy/services -Isrc/pregnancy/models -Isrc/ui -Isrc/ui/assets \
    test/kick_flow_test.cpp src/apps/kick/kick_flow.cpp src/apps/kick/kick_pages.cpp \
    src/pregnancy/services/kick_counter.cpp src/apps/pregnancy/pregnancy_state.cpp \
    src/ui/canvas.cpp src/ui/font.cpp src/ui/prototype_ui.cpp \
    src/ui/text_layout.cpp src/ui/ui_language.cpp \
    src/ui/assets/chinese_font_assets.cpp
compile_and_run sticky_shortcut_test -Isrc/app -Isrc/ui \
    test/sticky_shortcut_test.cpp src/app/sticky_shortcut.cpp
compile_and_run side_key_tracker_test -Isrc/input \
    test/side_key_tracker_test.cpp src/input/side_key_tracker.cpp
compile_and_run sticky_key_router_test -Isrc/app -Isrc/ui \
    test/sticky_key_router_test.cpp src/app/sticky_key_router.cpp
compile_and_run sticky_app_gesture_test -Isrc/app \
    test/sticky_app_gesture_test.cpp src/app/sticky_app_gesture.cpp
compile_and_run sticky_app_display_orientation_test \
    -Itest/support -Isrc/app -Isrc/sensors -Isrc/ui \
    test/sticky_app_display_orientation_test.cpp \
    src/app/sticky_app_display_orientation.cpp
compile_and_run sticky_battery_protocol_test -Isrc/devices \
    test/sticky_battery_protocol_test.cpp
compile_and_run sticky_touch_recovery_policy_test -Isrc/input \
    test/sticky_touch_recovery_policy_test.cpp \
    src/input/sticky_touch_recovery_policy.cpp
compile_and_run sticky_shake_detector_test -Isrc/sensors \
    test/sticky_shake_detector_test.cpp src/sensors/sticky_shake_detector.cpp
compile_and_run ui_language_test -Isrc/ui -Isrc/ui/assets \
    test/ui_language_test.cpp src/ui/canvas.cpp src/ui/font.cpp \
    src/ui/ui_language.cpp src/ui/assets/chinese_font_assets.cpp
compile_and_run text_layout_test -DUI_TEXT_OBSERVER -Isrc/ui -Isrc/ui/assets \
    test/text_layout_test.cpp src/ui/text_layout.cpp src/ui/canvas.cpp \
    src/ui/font.cpp src/ui/ui_language.cpp \
    src/ui/assets/chinese_font_assets.cpp

echo "[host-test] validate_sd_backup_test"
python3 test/validate_sd_backup_test.py
passed_count=$((passed_count + 1))

echo "[host-test] check_ui_tokens"
python3 tools/check_ui_tokens.py
passed_count=$((passed_count + 1))

echo "[host-test] PASS ${passed_count}/${passed_count}"
