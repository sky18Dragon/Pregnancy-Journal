#include "checkup_app.h"

#include <cstdio>
#include <cstring>

#include "app_log.h"
#include "canvas.h"
#include "checkup_service.h"
#include "checkup_storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pregnancy_storage.h"
#include "sticky_app_lifecycle.h"
#include "sticky_display.h"
#include "sticky_rtc.h"
#include "sticky_touch.h"
#include "ui_language.h"

namespace {
constexpr char kTag[] = "CHECKUP";
Canvas *s_canvas = nullptr;
TaskHandle_t s_task = nullptr;
StickyAppLifecycle s_lifecycle = {};
CheckupService s_service;
size_t s_selected = 0U;
bool s_adding = false;
PregnancyDate s_new_date = {};
uint32_t s_now = 0U;

bool read_today(PregnancyDate &today)
{
    StickyRtcDateTime clock = {};
    if (sticky_rtc_read(clock) != ESP_OK ||
        !sticky_rtc_epoch_seconds(clock, s_now)) return false;
    today = {clock.year, clock.month, clock.day};
    return true;
}

void button(int x, int y, int w, int h, const char *label, bool filled = false)
{
    if (filled) s_canvas->fill_rect(x, y, w, h, GrayLevel::Black);
    else s_canvas->draw_rect(x, y, w, h, GrayLevel::Black);
    s_canvas->draw_text(x + 12, y + (h - 14) / 2, ui_text(label), 2,
                        filled ? GrayLevel::White : GrayLevel::Black);
}

void render_list()
{
    s_canvas->set_rotation(CanvasRotation::Deg0);
    s_canvas->clear(GrayLevel::White);
    s_canvas->draw_text(34, 24, ui_text("CHECKUPS"), 5, GrayLevel::Black);
    s_canvas->draw_text(34, 76, ui_text("YOUR APPOINTMENTS"), 2,
                        GrayLevel::DarkGray);
    if (s_service.count() == 0U) {
        s_canvas->draw_text(54, 190, ui_text("NO CHECKUPS ADDED"), 4,
                            GrayLevel::Black);
        s_canvas->draw_text(54, 248, ui_text("REFERENCE PLANS VARY BY CARE TEAM"),
                            2, GrayLevel::DarkGray);
    }
    const size_t count = s_service.count() < 4U ? s_service.count() : 4U;
    for (size_t i = 0U; i < count; ++i) {
        const Checkup *item = s_service.at(i);
        const int y = 112 + static_cast<int>(i) * 66;
        const bool selected = i == s_selected;
        if (selected) s_canvas->fill_rect(34, y, 732, 56, GrayLevel::Black);
        else s_canvas->draw_rect(34, y, 732, 56, GrayLevel::Black);
        char line[112] = {};
        std::snprintf(line, sizeof(line), "%c %04u-%02u-%02u  %s",
                      item->completed ? 'X' : 'O',
                      static_cast<unsigned>(item->date.year),
                      static_cast<unsigned>(item->date.month),
                      static_cast<unsigned>(item->date.day), item->title);
        s_canvas->draw_text(50, y + 18, line, 2,
                            selected ? GrayLevel::White : GrayLevel::Black);
    }
    button(34, 408, 210, 48, "ADD", true);
    button(276, 408, 230, 48, "COMPLETE");
    button(538, 408, 228, 48, "DELETE");
}

void render_add()
{
    s_canvas->clear(GrayLevel::White);
    s_canvas->draw_text(34, 24, ui_text("ADD CHECKUP"), 5, GrayLevel::Black);
    s_canvas->draw_text(34, 88, ui_text("CONFIRM THE DATE WITH YOUR CARE TEAM"),
                        2, GrayLevel::DarkGray);
    char date[24] = {};
    std::snprintf(date, sizeof(date), "%04u-%02u-%02u",
                  static_cast<unsigned>(s_new_date.year),
                  static_cast<unsigned>(s_new_date.month),
                  static_cast<unsigned>(s_new_date.day));
    s_canvas->draw_text(220, 166, date, 6, GrayLevel::Black);
    button(70, 260, 190, 62, "- 1 DAY");
    button(305, 260, 190, 62, "+ 1 DAY");
    button(540, 260, 190, 62, "+ 7 DAYS");
    button(70, 370, 210, 58, "CANCEL");
    button(520, 370, 210, 58, "SAVE", true);
}

void render(bool full = false)
{
    if (s_adding) render_add(); else render_list();
    if (full) sticky_display_refresh_monochrome();
    else sticky_display_refresh_partial();
}

void shift_date(int days)
{
    int32_t index = 0;
    PregnancyDate shifted = {};
    if (pregnancy_date_to_day_index(s_new_date, index) &&
        pregnancy_date_from_day_index(index + days, shifted)) s_new_date = shifted;
}

void handle(int x, int y)
{
    if (s_adding) {
        if (y >= 260 && y < 322) {
            if (x < 280) shift_date(-1);
            else if (x < 520) shift_date(1);
            else shift_date(7);
        } else if (y >= 370 && y < 428 && x < 300) s_adding = false;
        else if (y >= 370 && y < 428 && x >= 500) {
            Checkup item = {};
            item.date = s_new_date;
            std::strcpy(item.title, "ROUTINE CHECKUP");
            std::strcpy(item.note, "CONFIRM DETAILS WITH YOUR CARE TEAM");
            if (s_service.create(item)) checkup_storage_save(s_service);
            s_adding = false;
        }
        render(); return;
    }
    const size_t count = s_service.count() < 4U ? s_service.count() : 4U;
    if (y >= 112 && y < 376) {
        const size_t index = static_cast<size_t>((y - 112) / 66);
        if (index < count) s_selected = index;
    } else if (y >= 408 && y < 456) {
        if (x < 244) {
            PregnancyDate today = {};
            if (read_today(today)) { s_new_date = today; shift_date(7); s_adding = true; }
        } else if (x < 506 && s_selected < count) {
            s_service.complete(s_service.at(s_selected)->id);
            checkup_storage_save(s_service);
        } else if (s_selected < count) {
            s_service.remove(s_service.at(s_selected)->id);
            checkup_storage_save(s_service);
            if (s_selected > 0U) --s_selected;
        }
    }
    render();
}

void task(void *)
{
    checkup_storage_load(s_service); render(true);
    while (true) {
        if (sticky_app_lifecycle_checkpoint(s_lifecycle)) {
            checkup_storage_load(s_service); render(true);
        }
        StickyTouchPress press = {};
        if (sticky_touch_take_press(press)) {
            int x = 0, y = 0; s_canvas->physical_to_logical(press.x, press.y, x, y);
            handle(x, y); sticky_touch_clear_press();
        }
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}
}  // namespace

esp_err_t checkup_app_start(Canvas &canvas)
{
    app_log_register_tag(kTag); if (s_task != nullptr) return ESP_OK; s_canvas = &canvas;
    return xTaskCreate(task, "checkup", 4608U, nullptr, 3U, &s_task) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
esp_err_t checkup_app_pause() { return sticky_app_lifecycle_pause(s_lifecycle, s_task); }
esp_err_t checkup_app_resume()
{ if (s_task == nullptr) return ESP_ERR_INVALID_STATE; sticky_app_lifecycle_resume(s_lifecycle); return ESP_OK; }
esp_err_t checkup_app_prepare_power_sleep(uint32_t &current, uint32_t &next)
{
    current = 0U;
    next = 0U;
    const esp_err_t result = checkup_app_pause();
    if (result != ESP_OK) return result;
    PregnancyDate today = {};
    read_today(today);
    current = s_now;
    s_service.next(current, &next);
    return ESP_OK;
}
uint32_t checkup_app_power_sleep_timeout_ms() { return s_adding ? 0U : 60U * 1000U; }
