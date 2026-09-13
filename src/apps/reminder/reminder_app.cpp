#include "reminder_app.h"

#include <cstdio>
#include <cstring>

#include "app_log.h"
#include "canvas.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "reminder_service.h"
#include "reminder_storage.h"
#include "sticky_app_lifecycle.h"
#include "sticky_display.h"
#include "sticky_rtc.h"
#include "sticky_touch.h"
#include "ui_language.h"

namespace {
constexpr char kTag[] = "REMINDER";
constexpr uint32_t kSleepTimeoutMs = 60U * 1000U;
constexpr size_t kVisibleRows = 4U;

Canvas *s_canvas = nullptr;
TaskHandle_t s_task = nullptr;
StickyAppLifecycle s_lifecycle = {};
ReminderService s_service;
bool s_today_mode = true;
bool s_adding = false;
size_t s_selected = 0U;
ReminderType s_new_type = ReminderType::General;
uint8_t s_new_hour = 9U;
uint8_t s_new_minute = 0U;
PregnancyDate s_today = {};
uint32_t s_now = 0U;

bool read_now()
{
    StickyRtcDateTime clock = {};
    if (sticky_rtc_read(clock) != ESP_OK ||
        !sticky_rtc_epoch_seconds(clock, s_now)) return false;
    s_today = {clock.year, clock.month, clock.day};
    return true;
}

void box(int x, int y, int width, int height, bool filled = false)
{
    if (filled) s_canvas->fill_rect(x, y, width, height, GrayLevel::Black);
    else s_canvas->draw_rect(x, y, width, height, GrayLevel::Black);
}

void button(int x, int y, int width, int height, const char *label, bool filled)
{
    box(x, y, width, height, filled);
    const GrayLevel color = filled ? GrayLevel::White : GrayLevel::Black;
    s_canvas->draw_text(x + 12, y + (height - 14) / 2, ui_text(label), 2, color);
}

size_t visible(const Reminder *items[kVisibleRows])
{
    if (s_today_mode) {
        Reminder *mutable_items[kVisibleRows] = {};
        const size_t count = s_service.get_today(s_today, mutable_items, kVisibleRows);
        for (size_t i = 0U; i < count; ++i) items[i] = mutable_items[i];
        return count;
    }
    return s_service.get_upcoming(s_now, items, kVisibleRows);
}

void render_list()
{
    s_canvas->set_rotation(CanvasRotation::Deg0);
    s_canvas->clear(GrayLevel::White);
    s_canvas->draw_text(34, 24, ui_text("REMINDERS"), 5, GrayLevel::Black);
    s_canvas->draw_text(34, 76,
                        ui_text(s_today_mode ? "TODAY" : "UPCOMING"), 2,
                        GrayLevel::DarkGray);
    const Reminder *items[kVisibleRows] = {};
    const size_t count = visible(items);
    if (count == 0U) {
        s_canvas->draw_text(54, 190, ui_text("NO REMINDERS"), 4,
                            GrayLevel::Black);
        s_canvas->draw_text(54, 245, ui_text("TAP ADD TO CREATE ONE"), 2,
                            GrayLevel::DarkGray);
    }
    for (size_t i = 0U; i < count; ++i) {
        const int y = 112 + static_cast<int>(i) * 66;
        const bool selected = i == s_selected;
        box(34, y, 732, 56, selected);
        const GrayLevel color = selected ? GrayLevel::White : GrayLevel::Black;
        char line[96] = {};
        std::snprintf(line, sizeof(line), "%c %02u:%02u  %s",
                      items[i]->completed ? 'X' : 'O',
                      static_cast<unsigned>(items[i]->hour),
                      static_cast<unsigned>(items[i]->minute), items[i]->title);
        s_canvas->draw_text(52, y + 18, line, 2, color);
    }
    button(34, 408, 154, 48, s_today_mode ? "UPCOMING" : "TODAY", false);
    button(208, 408, 154, 48, "ADD", true);
    button(382, 408, 176, 48, "COMPLETE", false);
    button(578, 408, 188, 48, "DELETE", false);
}

void render_add()
{
    s_canvas->clear(GrayLevel::White);
    s_canvas->draw_text(34, 24, ui_text("ADD REMINDER"), 5, GrayLevel::Black);
    const char *types[] = {"GENERAL", "SUPPLEMENT", "CHECKUP", "WATER", "ACTIVITY", "REST"};
    for (size_t i = 0U; i < 6U; ++i) {
        const int x = 34 + static_cast<int>(i % 3U) * 244;
        const int y = 92 + static_cast<int>(i / 3U) * 70;
        button(x, y, 224, 54, types[i], static_cast<size_t>(s_new_type) == i);
    }
    char time[16] = {};
    std::snprintf(time, sizeof(time), "%02u:%02u",
                  static_cast<unsigned>(s_new_hour),
                  static_cast<unsigned>(s_new_minute));
    s_canvas->draw_text(310, 264, time, 7, GrayLevel::Black);
    button(84, 270, 120, 58, "- HOUR", false);
    button(596, 270, 120, 58, "+ HOUR", false);
    button(84, 350, 180, 58, "CANCEL", false);
    button(310, 350, 180, 58, "SAVE", true);
}

void render(bool full = false)
{
    read_now();
    if (s_adding) render_add(); else render_list();
    if (full) sticky_display_refresh_monochrome();
    else sticky_display_refresh_partial();
}

void choose_type(size_t index)
{
    s_new_type = static_cast<ReminderType>(index);
}

void save_new()
{
    Reminder item = {};
    item.type = s_new_type;
    item.date = s_today;
    item.hour = s_new_hour;
    item.minute = s_new_minute;
    std::snprintf(item.title, sizeof(item.title), "%s",
                  reminder_type_name(item.type));
    if (s_service.create(item)) reminder_storage_save(s_service);
    s_adding = false;
    s_selected = 0U;
}

void handle_press(int x, int y)
{
    if (s_adding) {
        if (y >= 92 && y < 216) {
            const int column = (x - 34) / 244;
            const int row = (y - 92) / 70;
            const int index = row * 3 + column;
            if (column >= 0 && column < 3 && row >= 0 && row < 2)
                choose_type(static_cast<size_t>(index));
        } else if (x >= 84 && x < 204 && y >= 270 && y < 328) {
            s_new_hour = static_cast<uint8_t>((s_new_hour + 23U) % 24U);
        } else if (x >= 596 && x < 716 && y >= 270 && y < 328) {
            s_new_hour = static_cast<uint8_t>((s_new_hour + 1U) % 24U);
        } else if (x >= 84 && x < 264 && y >= 350 && y < 408) {
            s_adding = false;
        } else if (x >= 310 && x < 490 && y >= 350 && y < 408) {
            save_new();
        }
        render();
        return;
    }
    const Reminder *items[kVisibleRows] = {};
    const size_t count = visible(items);
    if (y >= 112 && y < 376) {
        const size_t index = static_cast<size_t>((y - 112) / 66);
        if (index < count) s_selected = index;
    } else if (y >= 408 && y < 456) {
        if (x < 188) { s_today_mode = !s_today_mode; s_selected = 0U; }
        else if (x >= 208 && x < 362) {
            s_adding = true;
            StickyRtcDateTime clock = {};
            if (sticky_rtc_read(clock) == ESP_OK)
                s_new_hour = static_cast<uint8_t>((clock.hour + 1U) % 24U);
        } else if (x >= 382 && x < 558 && s_selected < count) {
            s_service.complete(items[s_selected]->id);
            reminder_storage_save(s_service);
        } else if (x >= 578 && s_selected < count) {
            s_service.remove(items[s_selected]->id);
            reminder_storage_save(s_service);
            if (s_selected > 0U) --s_selected;
        }
    }
    render();
}

void task(void *)
{
    reminder_storage_load(s_service);
    render(true);
    while (true) {
        if (sticky_app_lifecycle_checkpoint(s_lifecycle)) {
            reminder_storage_load(s_service);
            render(true);
        }
        StickyTouchPress press = {};
        if (sticky_touch_take_press(press)) {
            int x = 0, y = 0;
            s_canvas->physical_to_logical(press.x, press.y, x, y);
            handle_press(x, y);
            sticky_touch_clear_press();
        }
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}
}  // namespace

esp_err_t reminder_app_start(Canvas &canvas)
{
    app_log_register_tag(kTag);
    if (s_task != nullptr) return ESP_OK;
    s_canvas = &canvas;
    return xTaskCreate(task, "reminder", 5120U, nullptr, 3U, &s_task) == pdPASS
               ? ESP_OK : ESP_ERR_NO_MEM;
}
esp_err_t reminder_app_pause() { return sticky_app_lifecycle_pause(s_lifecycle, s_task); }
esp_err_t reminder_app_resume()
{ if (s_task == nullptr) return ESP_ERR_INVALID_STATE; sticky_app_lifecycle_resume(s_lifecycle); return ESP_OK; }
esp_err_t reminder_app_prepare_power_sleep(uint32_t &current, uint32_t &next)
{
    current = 0U; next = 0U;
    const esp_err_t result = reminder_app_pause();
    if (result != ESP_OK) return result;
    read_now();
    current = s_now;
    s_service.next(current, &next);
    return ESP_OK;
}
uint32_t reminder_app_power_sleep_timeout_ms() { return s_adding ? 0U : kSleepTimeoutMs; }
