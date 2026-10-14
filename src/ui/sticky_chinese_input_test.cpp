#include "sticky_chinese_input_test.h"

#include <cstdint>

#include "app_log.h"
#include "chinese_input_page.h"
#include "pinyin_candidate_index.h"
#include "sticky_display.h"
#include "sticky_touch.h"

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef STICKY_CHINESE_INPUT_TEST_PORTRAIT
#define STICKY_CHINESE_INPUT_TEST_PORTRAIT 0
#endif

namespace {

constexpr char kTag[] = "chinese_input_test";
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(30);
constexpr uint8_t kPartialRefreshesBeforeCleanup = 12U;

struct HeapSnapshot {
    size_t internal_free;
    size_t internal_minimum;
    size_t psram_free;
    size_t psram_minimum;
};

HeapSnapshot heap_snapshot()
{
    return {
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM),
    };
}

void log_heap(const char *phase, const HeapSnapshot &snapshot,
              const HeapSnapshot *baseline = nullptr)
{
    const int internal_delta =
        baseline == nullptr
            ? 0
            : static_cast<int>(snapshot.internal_free) -
                  static_cast<int>(baseline->internal_free);
    const int psram_delta =
        baseline == nullptr
            ? 0
            : static_cast<int>(snapshot.psram_free) -
                  static_cast<int>(baseline->psram_free);
    STICKY_LOGI(
        kTag,
        "memory=heap phase=%s internal_free=%u internal_minimum=%u internal_delta=%d psram_free=%u psram_minimum=%u psram_delta=%d",
        phase, static_cast<unsigned>(snapshot.internal_free),
        static_cast<unsigned>(snapshot.internal_minimum), internal_delta,
        static_cast<unsigned>(snapshot.psram_free),
        static_cast<unsigned>(snapshot.psram_minimum), psram_delta);
}

void seed_editor(sticky_ui::ChineseTextEditor &editor)
{
    (void)editor.set_text("排序验收：");
    (void)editor.type_pinyin('y');
    (void)editor.type_pinyin('u');
    (void)editor.type_pinyin('n');
}

esp_err_t refresh_after_action(uint8_t &partial_refresh_count)
{
    ++partial_refresh_count;
    if (partial_refresh_count >= kPartialRefreshesBeforeCleanup) {
        partial_refresh_count = 0U;
        return sticky_display_refresh_monochrome();
    }
    return sticky_display_refresh_partial();
}

}  // namespace

[[noreturn]] void sticky_chinese_input_test_run(Canvas &canvas)
{
    app_log_register_tag(kTag);
    sticky_display_set_battery_overlay_enabled(false);
#if STICKY_CHINESE_INPUT_TEST_PORTRAIT
    canvas.set_rotation(CanvasRotation::Deg90Clockwise);
#endif
    sticky_ui::ChineseTextEditor editor;
    seed_editor(editor);
    bool limit_reached = false;
    bool saved = false;
    uint8_t partial_refresh_count = 0U;
    const HeapSnapshot before_render = heap_snapshot();
    log_heap("before_render", before_render);
    sticky_ui::chinese_input_page_render(canvas, editor, limit_reached, saved);
    const HeapSnapshot after_render = heap_snapshot();
    log_heap("after_render", after_render, &before_render);
    esp_err_t result = sticky_display_refresh_monochrome();
    const HeapSnapshot after_refresh = heap_snapshot();
    log_heap("after_refresh", after_refresh, &before_render);
    STICKY_LOGI(kTag,
                "phase=ready orientation=%s logical_width=%u logical_height=%u refresh=%s result=%s",
#if STICKY_CHINESE_INPUT_TEST_PORTRAIT
                "portrait",
#else
                "landscape",
#endif
                static_cast<unsigned>(canvas.width()),
                static_cast<unsigned>(canvas.height()), "full_monochrome",
                esp_err_to_name(result));
    STICKY_LOGI(
        kTag,
        "ranking_version=%u domain_first_page=%u/%u domain_max_rank=%u",
        static_cast<unsigned>(pinyin_candidate_ranking_version()),
        static_cast<unsigned>(pinyin_candidate_domain_first_page_count()),
        static_cast<unsigned>(pinyin_candidate_domain_benchmark_count()),
        static_cast<unsigned>(pinyin_candidate_domain_maximum_rank()));

    while (true) {
        StickyTouchPress press = {};
        if (!sticky_touch_take_press(press)) {
            vTaskDelay(kPollInterval);
            continue;
        }
        int logical_x = 0;
        int logical_y = 0;
        canvas.physical_to_logical(press.x, press.y, logical_x, logical_y);
        const sticky_ui::ChineseInputAction action =
            sticky_ui::chinese_input_page_action_at(
                canvas.width(), canvas.height(), logical_x, logical_y);
        if (action == sticky_ui::ChineseInputAction::None) {
            continue;
        }

        sticky_ui::ChineseEditorResult editor_result =
            sticky_ui::ChineseEditorResult::Ok;
        if (action == sticky_ui::ChineseInputAction::Save) {
            saved = true;
        } else if (action == sticky_ui::ChineseInputAction::Cancel) {
            editor.clear_text();
            editor.clear_pinyin();
            saved = false;
        } else {
            saved = false;
            editor_result =
                sticky_ui::chinese_input_page_apply_action(editor, action);
        }
        limit_reached = editor_result == sticky_ui::ChineseEditorResult::Full;
        sticky_ui::chinese_input_page_render(canvas, editor, limit_reached,
                                             saved);
        result = refresh_after_action(partial_refresh_count);
        STICKY_LOGI(
            kTag,
            "touch=physical_x%u_y%u logical_x%d_y%d action=%u editor_result=%u text_count=%u pinyin=%s refresh=%s result=%s",
            static_cast<unsigned>(press.x), static_cast<unsigned>(press.y),
            logical_x, logical_y,
            static_cast<unsigned>(action),
            static_cast<unsigned>(editor_result),
            static_cast<unsigned>(editor.codepoint_count()), editor.pinyin(),
            partial_refresh_count == 0U ? "full_monochrome" : "partial",
            esp_err_to_name(result));
    }
}
