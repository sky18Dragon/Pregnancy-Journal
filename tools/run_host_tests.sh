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

compile_and_run app_manager_test -Itest/support -Isrc/app \
    test/app_manager_test.cpp src/app/app_manager.cpp
compile_and_run app_launcher_test -Isrc/app -Isrc/ui -Isrc/ui/assets \
    test/app_launcher_test.cpp src/ui/app_pages.cpp src/ui/canvas.cpp \
    src/ui/font.cpp src/ui/ui_language.cpp \
    src/ui/assets/chinese_font_assets.cpp
compile_and_run persistent_state_test -Isrc/app -Isrc/storage -Isrc/ui \
    test/persistent_state_test.cpp src/storage/persistent_state.cpp \
    src/app/sticky_app_id.cpp
compile_and_run scheduler_test -Isrc/system \
    test/scheduler_test.cpp src/system/scheduler.cpp
compile_and_run home_pages_test -Isrc/apps/home -Isrc/ui -Isrc/ui/assets \
    test/home_pages_test.cpp src/apps/home/home_pages.cpp \
    src/ui/canvas.cpp src/ui/font.cpp src/ui/ui_language.cpp \
    src/ui/assets/chinese_font_assets.cpp
compile_and_run settings_pages_test -Isrc/apps/settings -Isrc/ui -Isrc/ui/assets \
    test/settings_pages_test.cpp src/apps/settings/settings_pages.cpp \
    src/ui/canvas.cpp src/ui/font.cpp src/ui/ui_language.cpp \
    src/ui/assets/chinese_font_assets.cpp
compile_and_run pregnancy_state_test -Isrc/apps/pregnancy \
    test/pregnancy_state_test.cpp src/apps/pregnancy/pregnancy_state.cpp
compile_and_run pregnancy_storage_record_test -Isrc/apps/pregnancy \
    test/pregnancy_storage_record_test.cpp \
    src/apps/pregnancy/pregnancy_storage_record.cpp \
    src/apps/pregnancy/pregnancy_state.cpp
compile_and_run pregnancy_pages_test \
    -Isrc/apps/pregnancy -Isrc/app -Isrc/ui -Isrc/ui/assets \
    test/pregnancy_pages_test.cpp src/apps/pregnancy/pregnancy_pages.cpp \
    src/apps/pregnancy/pregnancy_state.cpp src/ui/canvas.cpp src/ui/font.cpp \
    src/ui/ui_language.cpp src/ui/assets/chinese_font_assets.cpp \
    src/ui/assets/pixel_asset.cpp src/ui/assets/app_launcher_assets.cpp
compile_and_run sticky_app_gesture_test -Isrc/app \
    test/sticky_app_gesture_test.cpp src/app/sticky_app_gesture.cpp
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

echo "[host-test] PASS ${passed_count}/${passed_count}"
