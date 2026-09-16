#include "reminder_app.h"

#include <cstdio>
#include <cstring>

#include "app_log.h"
#include "canvas.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "prototype_ui.h"
#include "reminder_service.h"
#include "reminder_storage.h"
#include "sticky_app.h"
#include "sticky_app_lifecycle.h"
#include "sticky_display.h"
#include "sticky_rtc.h"
#include "sticky_touch.h"
#include "ui_language.h"
#include "ui_tokens.h"

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

bool portrait() { return s_canvas->height() > s_canvas->width(); }

prototype_ui::Rect mode_button_rect(size_t index) {
  if (portrait())
    return {24 + static_cast<int>(index) * 224, 96, 208, 48};
  return {448 + static_cast<int>(index) * 92, 20, 92,
          ui_tokens::kMinimumTouchHeight};
}

prototype_ui::Rect list_row_rect(size_t index) {
  if (portrait())
    return {24, 164 + static_cast<int>(index) * 116, 432, 104};
  return {24, 88 + static_cast<int>(index) * 64, 752, 64};
}

prototype_ui::Rect footer_button_rect(size_t index) {
  if (portrait())
    return {24 + static_cast<int>(index) * 148, 720, 136, 56};
  return {24 + static_cast<int>(index) * 256, 400, 240, 56};
}

prototype_ui::Rect type_button_rect(size_t index) {
  if (portrait())
    return {24 + static_cast<int>(index % 2U) * 224,
            112 + static_cast<int>(index / 2U) * 68, 208, 56};
  return {24 + static_cast<int>(index % 3U) * 254,
          100 + static_cast<int>(index / 3U) * 68, 242, 56};
}

bool read_now() {
  StickyRtcDateTime clock = {};
  if (sticky_rtc_read(clock) != ESP_OK ||
      !sticky_rtc_epoch_seconds(clock, s_now))
    return false;
  s_today = {clock.year, clock.month, clock.day};
  return true;
}

void button(int x, int y, int width, int height, const char *label,
            bool filled) {
  prototype_ui::draw_button(*s_canvas, {x, y, width, height}, label, filled, 2);
}

size_t visible(const Reminder *items[kVisibleRows]) {
  if (s_today_mode) {
    Reminder *mutable_items[kVisibleRows] = {};
    const size_t count =
        s_service.get_today(s_today, mutable_items, kVisibleRows);
    for (size_t i = 0U; i < count; ++i)
      items[i] = mutable_items[i];
    return count;
  }
  return s_service.get_upcoming(s_now, items, kVisibleRows);
}

void render_list() {
  prototype_ui::begin_page(*s_canvas);
  prototype_ui::draw_header(*s_canvas, "REMINDERS",
                            "TODAY'S TASKS AND UPCOMING ALERTS", "BACK");
  const prototype_ui::Rect today = mode_button_rect(0U);
  const prototype_ui::Rect upcoming = mode_button_rect(1U);
  button(today.x, today.y, today.width, today.height, "TODAY", s_today_mode);
  button(upcoming.x, upcoming.y, upcoming.width, upcoming.height, "UPCOMING",
         !s_today_mode);
  const Reminder *items[kVisibleRows] = {};
  const size_t count = visible(items);
  if (count == 0U) {
    s_canvas->draw_text(32, portrait() ? 236 : 164, ui_text("NO REMINDERS"), 4,
                        GrayLevel::Black);
    s_canvas->draw_text(32, portrait() ? 286 : 214,
                        ui_text("TAP ADD TO CREATE ONE"), 2, GrayLevel::Black);
  }
  for (size_t i = 0U; i < count; ++i) {
    const prototype_ui::Rect row = list_row_rect(i);
    const int y = row.y;
    const bool selected = i == s_selected;
    if (selected)
      s_canvas->fill_rect(row.x, row.y, row.width, row.height,
                          GrayLevel::Black);
    else {
      s_canvas->draw_line(row.x, row.y, row.x + row.width, row.y,
                          GrayLevel::Black);
      s_canvas->draw_line(row.x, row.y + row.height - 1, row.x + row.width,
                          row.y + row.height - 1, GrayLevel::Black);
    }
    const GrayLevel color = selected ? GrayLevel::White : GrayLevel::Black;
    const int status_y = y + (portrait() ? 45 : 25);
    prototype_ui::draw_status_square(*s_canvas, 32, status_y,
                                     items[i]->completed, color);
    char time[8] = {};
    std::snprintf(time, sizeof(time), "%02u:%02u",
                  static_cast<unsigned>(items[i]->hour),
                  static_cast<unsigned>(items[i]->minute));
    s_canvas->draw_text(60, y + (portrait() ? 18 : 20), time, 2, color);
    const int copy_x = portrait() ? 148 : 132;
    s_canvas->draw_text(copy_x, y + 14, ui_text(items[i]->title), 2, color);
    s_canvas->draw_text(copy_x, y + (portrait() ? 50 : 38),
                        ui_text(items[i]->completed ? "COMPLETED" : "TODAY"), 1,
                        selected ? GrayLevel::White : GrayLevel::Black);
    prototype_ui::draw_chevron(*s_canvas, row.x + row.width - 24,
                               y + (row.height - 16) / 2, color);
  }
  const char *labels[] = {"ADD REMINDER", "COMPLETE", "DELETE"};
  for (size_t i = 0U; i < 3U; ++i) {
    const prototype_ui::Rect rect = footer_button_rect(i);
    button(rect.x, rect.y, rect.width, rect.height, labels[i], i == 0U);
  }
}

void render_add() {
  prototype_ui::begin_page(*s_canvas);
  prototype_ui::draw_header(*s_canvas, "ADD REMINDER", "CHOOSE A TYPE AND TIME",
                            "CANCEL");
  const char *types[] = {"GENERAL", "SUPPLEMENT", "CHECKUP",
                         "WATER",   "ACTIVITY",   "REST"};
  for (size_t i = 0U; i < 6U; ++i) {
    const prototype_ui::Rect rect = type_button_rect(i);
    button(rect.x, rect.y, rect.width, rect.height, types[i],
           static_cast<size_t>(s_new_type) == i);
  }
  char time[16] = {};
  std::snprintf(time, sizeof(time), "%02u:%02u",
                static_cast<unsigned>(s_new_hour),
                static_cast<unsigned>(s_new_minute));
  const int time_y = portrait() ? 356 : 248;
  s_canvas->draw_text((s_canvas->width() - ui_text_width(time, 8U)) / 2, time_y,
                      time, 8, GrayLevel::Black);
  const char *note = ui_text("REMINDER WILL RUN TODAY");
  s_canvas->draw_text((s_canvas->width() - ui_text_width(note, 1U)) / 2,
                      portrait() ? 430 : 316, note, 1, GrayLevel::Black);
  const char *labels[] = {"- 1 HOUR", "+ 1 HOUR", "SAVE REMINDER"};
  for (size_t i = 0U; i < 3U; ++i) {
    const prototype_ui::Rect rect = footer_button_rect(i);
    button(rect.x, rect.y, rect.width, rect.height, labels[i], i == 2U);
  }
}

void render(bool full = false) {
  sticky_display_set_battery_overlay_enabled(false);
  read_now();
  if (s_adding)
    render_add();
  else
    render_list();
  if (full)
    sticky_display_refresh_monochrome();
  else
    sticky_display_refresh_partial();
}

void choose_type(size_t index) {
  s_new_type = static_cast<ReminderType>(index);
}

void save_new() {
  Reminder item = {};
  item.type = s_new_type;
  item.date = s_today;
  item.hour = s_new_hour;
  item.minute = s_new_minute;
  std::snprintf(item.title, sizeof(item.title), "%s",
                reminder_type_name(item.type));
  if (s_service.create(item))
    reminder_storage_save(s_service);
  s_adding = false;
  s_selected = 0U;
}

void handle_press(int x, int y) {
  const prototype_ui::Rect back =
      prototype_ui::header_action_rect(s_canvas->width(), s_canvas->height());
  if (s_adding) {
    if (back.contains(x, y)) {
      s_adding = false;
    } else {
      for (size_t i = 0U; i < 6U; ++i) {
        if (type_button_rect(i).contains(x, y)) {
          choose_type(i);
          render();
          return;
        }
      }
      if (footer_button_rect(0U).contains(x, y)) {
        s_new_hour = static_cast<uint8_t>((s_new_hour + 23U) % 24U);
      } else if (footer_button_rect(1U).contains(x, y)) {
        s_new_hour = static_cast<uint8_t>((s_new_hour + 1U) % 24U);
      } else if (footer_button_rect(2U).contains(x, y)) {
        save_new();
      }
    }
    render();
    return;
  }
  if (back.contains(x, y)) {
    sticky_app_request_launcher();
    return;
  }
  for (size_t i = 0U; i < 2U; ++i) {
    if (mode_button_rect(i).contains(x, y)) {
      s_today_mode = i == 0U;
      s_selected = 0U;
      render();
      return;
    }
  }
  const Reminder *items[kVisibleRows] = {};
  const size_t count = visible(items);
  for (size_t i = 0U; i < count; ++i) {
    if (list_row_rect(i).contains(x, y)) {
      s_selected = i;
      render();
      return;
    }
  }
  if (footer_button_rect(0U).contains(x, y)) {
    s_adding = true;
    StickyRtcDateTime clock = {};
    if (sticky_rtc_read(clock) == ESP_OK)
      s_new_hour = static_cast<uint8_t>((clock.hour + 1U) % 24U);
  } else if (footer_button_rect(1U).contains(x, y) && s_selected < count) {
    s_service.complete(items[s_selected]->id);
    reminder_storage_save(s_service);
  } else if (footer_button_rect(2U).contains(x, y) && s_selected < count) {
    s_service.remove(items[s_selected]->id);
    reminder_storage_save(s_service);
    if (s_selected > 0U)
      --s_selected;
  }
  render();
}

void task(void *) {
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
} // namespace

esp_err_t reminder_app_start(Canvas &canvas) {
  app_log_register_tag(kTag);
  if (s_task != nullptr)
    return ESP_OK;
  s_canvas = &canvas;
  return xTaskCreate(task, "reminder", 5120U, nullptr, 3U, &s_task) == pdPASS
             ? ESP_OK
             : ESP_ERR_NO_MEM;
}
esp_err_t reminder_app_pause() {
  return sticky_app_lifecycle_pause(s_lifecycle, s_task);
}
esp_err_t reminder_app_resume() {
  if (s_task == nullptr)
    return ESP_ERR_INVALID_STATE;
  sticky_app_lifecycle_resume(s_lifecycle);
  return ESP_OK;
}
esp_err_t reminder_app_prepare_power_sleep(uint32_t &current, uint32_t &next) {
  current = 0U;
  next = 0U;
  const esp_err_t result = reminder_app_pause();
  if (result != ESP_OK)
    return result;
  read_now();
  current = s_now;
  s_service.next(current, &next);
  return ESP_OK;
}
uint32_t reminder_app_power_sleep_timeout_ms() {
  return s_adding ? 0U : kSleepTimeoutMs;
}
