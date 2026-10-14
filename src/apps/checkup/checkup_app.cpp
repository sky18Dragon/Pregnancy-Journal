#include "checkup_app.h"

#include <cstdio>
#include <cstring>

#include "app_log.h"
#include "canvas.h"
#include "checkup_service.h"
#include "domain_repository_stores.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "prototype_ui.h"
#include "repository_status_page.h"
#include "sticky_app.h"
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
pregnancy_repository::CheckupRepositoryStore &s_store =
    pregnancy_repository::checkup_repository_store();
sticky_storage::SdRepositoryAdapterStatus s_storage_status =
    sticky_storage::SdRepositoryAdapterStatus::Idle;
bool s_storage_saving = false;
bool s_reload_after_pending = false;
size_t s_selected = 0U;
bool s_adding = false;
PregnancyDate s_new_date = {};
uint32_t s_now = 0U;

bool portrait() { return s_canvas->height() > s_canvas->width(); }

prototype_ui::Rect row_rect(size_t index) {
  if (portrait())
    return {24, 136 + static_cast<int>(index) * 124, 432, 112};
  return {24, 88 + static_cast<int>(index) * 64, 752, 64};
}

prototype_ui::Rect footer_button_rect(size_t index) {
  if (portrait())
    return {24 + static_cast<int>(index) * 148, 720, 136, 56};
  return {24 + static_cast<int>(index) * 256, 400, 240, 56};
}

prototype_ui::Rect date_button_rect(size_t index) {
  if (portrait())
    return {24 + static_cast<int>(index) * 148, 326, 136, 56};
  return {128 + static_cast<int>(index) * 187, 250, 170, 48};
}

bool read_today(PregnancyDate &today) {
  StickyRtcDateTime clock = {};
  if (sticky_rtc_read(clock) != ESP_OK ||
      !sticky_rtc_epoch_seconds(clock, s_now))
    return false;
  today = {clock.year, clock.month, clock.day};
  return true;
}

void button(int x, int y, int w, int h, const char *label,
            bool filled = false) {
  prototype_ui::draw_button(*s_canvas, {x, y, w, h}, label, filled, 2);
}

void render_list() {
  const CheckupService &service = s_store.service();
  prototype_ui::begin_page(*s_canvas);
  prototype_ui::draw_header(*s_canvas, "CHECKUPS",
                            "CONFIRM APPOINTMENTS WITH YOUR CARE TEAM", "BACK");
  if (service.count() == 0U) {
    s_canvas->draw_text(32, portrait() ? 232 : 164,
                        ui_text("NO CHECKUPS ADDED"), 4, GrayLevel::Black);
    s_canvas->draw_text(32, portrait() ? 282 : 214,
                        ui_text("REFERENCE PLANS VARY BY CARE TEAM"), 2,
                        GrayLevel::Black);
  }
  const size_t count = service.count() < 4U ? service.count() : 4U;
  for (size_t i = 0U; i < count; ++i) {
    const Checkup *item = service.at(i);
    const prototype_ui::Rect row = row_rect(i);
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
    prototype_ui::draw_status_square(*s_canvas, 32, y + (portrait() ? 49 : 25),
                                     item->completed, color);
    char date[24] = {};
    std::snprintf(date, sizeof(date), "%04u.%02u.%02u",
                  static_cast<unsigned>(item->date.year),
                  static_cast<unsigned>(item->date.month),
                  static_cast<unsigned>(item->date.day));
    s_canvas->draw_text(60, y + (portrait() ? 18 : 20), date, 2, color);
    const int copy_x = portrait() ? 60 : 218;
    s_canvas->draw_text(copy_x, y + (portrait() ? 50 : 14),
                        ui_text(item->title), 2, color);
    s_canvas->draw_text(copy_x, y + (portrait() ? 78 : 38),
                        ui_text(item->completed ? "COMPLETED" : "09:00"), 1,
                        selected ? GrayLevel::White : GrayLevel::Black);
    prototype_ui::draw_chevron(*s_canvas, row.x + row.width - 24,
                               y + (row.height - 16) / 2, color);
  }
  const char *labels[] = {"ADD CHECKUP", "COMPLETE", "DELETE"};
  for (size_t i = 0U; i < 3U; ++i) {
    const prototype_ui::Rect rect = footer_button_rect(i);
    button(rect.x, rect.y, rect.width, rect.height, labels[i], i == 0U);
  }
}

void render_add() {
  prototype_ui::begin_page(*s_canvas);
  prototype_ui::draw_header(*s_canvas, "ADD CHECKUP",
                            "DEFAULTS TO TODAY PLUS 7 DAYS", "CANCEL");
  char date[24] = {};
  std::snprintf(date, sizeof(date), "%04u.%02u.%02u",
                static_cast<unsigned>(s_new_date.year),
                static_cast<unsigned>(s_new_date.month),
                static_cast<unsigned>(s_new_date.day));
  const uint8_t date_scale = portrait() ? 4U : 7U;
  s_canvas->draw_text((s_canvas->width() - ui_text_width(date, date_scale)) / 2,
                      portrait() ? 174 : 142, date, date_scale,
                      GrayLevel::Black);
  const char *labels[] = {"- 1 DAY", "+ 1 DAY", "+ 7 DAYS"};
  for (size_t i = 0U; i < 3U; ++i) {
    const prototype_ui::Rect rect = date_button_rect(i);
    button(rect.x, rect.y, rect.width, rect.height, labels[i]);
  }
  const char *note = ui_text("DEFAULT 09:00 · ROUTINE CHECKUP");
  s_canvas->draw_text((s_canvas->width() - ui_text_width(note, 1U)) / 2,
                      portrait() ? 414 : 324, note, 1, GrayLevel::Black);
  const prototype_ui::Rect save = portrait()
                                      ? prototype_ui::Rect{24, 720, 432, 56}
                                      : prototype_ui::Rect{24, 400, 752, 56};
  button(save.x, save.y, save.width, save.height, "SAVE CHECKUP", true);
}

void render(bool full = false) {
  sticky_display_set_battery_overlay_enabled(false);
  if (s_store.pending() ||
      (s_storage_status != sticky_storage::SdRepositoryAdapterStatus::Idle &&
       s_storage_status != sticky_storage::SdRepositoryAdapterStatus::Ok)) {
    repository_status_ui::render(*s_canvas, "CHECKUP DATA", s_store.pending(),
                                 s_storage_saving, s_storage_status);
  } else if (s_adding)
    render_add();
  else
    render_list();
  if (full)
    sticky_display_refresh_monochrome();
  else
    sticky_display_refresh_partial();
}

void begin_load() {
  s_adding = false;
  s_storage_saving = false;
  s_storage_status = s_store.begin_load();
  if (s_storage_status == sticky_storage::SdRepositoryAdapterStatus::Pending)
    s_storage_status = sticky_storage::SdRepositoryAdapterStatus::Idle;
}

void begin_save(const Checkup &item) {
  s_storage_status = s_store.begin_upsert(item);
  s_storage_saving =
      s_storage_status == sticky_storage::SdRepositoryAdapterStatus::Pending;
  if (s_storage_saving)
    s_storage_status = sticky_storage::SdRepositoryAdapterStatus::Idle;
}

void begin_delete(uint32_t id) {
  s_storage_status = s_store.begin_erase(id);
  s_storage_saving =
      s_storage_status == sticky_storage::SdRepositoryAdapterStatus::Pending;
  if (s_storage_saving)
    s_storage_status = sticky_storage::SdRepositoryAdapterStatus::Idle;
}

bool poll_store() {
  if (!s_store.pending()) return false;
  const sticky_storage::SdRepositoryAdapterStatus status = s_store.poll();
  if (status == sticky_storage::SdRepositoryAdapterStatus::Pending)
    return false;
  s_storage_saving = false;
  s_storage_status = status == sticky_storage::SdRepositoryAdapterStatus::Ok
                         ? sticky_storage::SdRepositoryAdapterStatus::Idle
                         : status;
  if (s_reload_after_pending) {
    s_reload_after_pending = false;
    begin_load();
  }
  return true;
}

void shift_date(int days) {
  int32_t index = 0;
  PregnancyDate shifted = {};
  if (pregnancy_date_to_day_index(s_new_date, index) &&
      pregnancy_date_from_day_index(index + days, shifted))
    s_new_date = shifted;
}

void handle(int x, int y) {
  const prototype_ui::Rect back =
      prototype_ui::header_action_rect(s_canvas->width(), s_canvas->height());
  if (s_store.pending()) return;
  if (s_storage_status != sticky_storage::SdRepositoryAdapterStatus::Idle &&
      s_storage_status != sticky_storage::SdRepositoryAdapterStatus::Ok) {
    if (back.contains(x, y)) sticky_app_request_launcher();
    else if (repository_status_ui::retry_rect(*s_canvas).contains(x, y))
      begin_load();
    render();
    return;
  }
  if (s_adding) {
    if (back.contains(x, y))
      s_adding = false;
    else if (date_button_rect(0U).contains(x, y))
      shift_date(-1);
    else if (date_button_rect(1U).contains(x, y))
      shift_date(1);
    else if (date_button_rect(2U).contains(x, y))
      shift_date(7);
    else if ((portrait() ? prototype_ui::Rect{24, 720, 432, 56}
                         : prototype_ui::Rect{24, 400, 752, 56})
                 .contains(x, y)) {
      Checkup item = {};
      item.date = s_new_date;
      std::strcpy(item.title, "ROUTINE CHECKUP");
      std::strcpy(item.note, "CONFIRM DETAILS WITH YOUR CARE TEAM");
      begin_save(item);
      if (s_store.pending()) s_adding = false;
    }
    render();
    return;
  }
  if (back.contains(x, y)) {
    sticky_app_request_launcher();
    return;
  }
  const CheckupService &service = s_store.service();
  const size_t count = service.count() < 4U ? service.count() : 4U;
  for (size_t i = 0U; i < count; ++i) {
    if (row_rect(i).contains(x, y)) {
      s_selected = i;
      render();
      return;
    }
  }
  if (footer_button_rect(0U).contains(x, y)) {
    PregnancyDate today = {};
    if (read_today(today)) {
      s_new_date = today;
      shift_date(7);
      s_adding = true;
    }
  } else if (footer_button_rect(1U).contains(x, y) && s_selected < count) {
    Checkup updated = *service.at(s_selected);
    updated.completed = true;
    begin_save(updated);
  } else if (footer_button_rect(2U).contains(x, y) && s_selected < count) {
    begin_delete(service.at(s_selected)->id);
    if (s_selected > 0U)
      --s_selected;
  }
  render();
}

void task(void *) {
  begin_load();
  render(true);
  while (true) {
    if (poll_store()) render(true);
    if (sticky_app_lifecycle_checkpoint(s_lifecycle)) {
      if (s_store.pending()) s_reload_after_pending = true;
      else begin_load();
      render(true);
    }
    StickyTouchPress press = {};
    if (sticky_touch_take_press(press)) {
      int x = 0, y = 0;
      s_canvas->physical_to_logical(press.x, press.y, x, y);
      handle(x, y);
      sticky_touch_clear_press();
    }
    vTaskDelay(pdMS_TO_TICKS(40));
  }
}
} // namespace

esp_err_t checkup_app_start(Canvas &canvas) {
  app_log_register_tag(kTag);
  if (s_task != nullptr)
    return ESP_OK;
  s_canvas = &canvas;
  return xTaskCreate(task, "checkup", 4608U, nullptr, 3U, &s_task) == pdPASS
             ? ESP_OK
             : ESP_ERR_NO_MEM;
}
esp_err_t checkup_app_pause() {
  return sticky_app_lifecycle_pause(s_lifecycle, s_task);
}
esp_err_t checkup_app_resume() {
  if (s_task == nullptr)
    return ESP_ERR_INVALID_STATE;
  sticky_app_lifecycle_resume(s_lifecycle);
  return ESP_OK;
}
esp_err_t checkup_app_prepare_power_sleep(uint32_t &current, uint32_t &next) {
  current = 0U;
  next = 0U;
  const esp_err_t result = checkup_app_pause();
  if (result != ESP_OK)
    return result;
  PregnancyDate today = {};
  read_today(today);
  current = s_now;
  s_store.service().next(current, &next);
  return ESP_OK;
}
uint32_t checkup_app_power_sleep_timeout_ms() {
  return s_adding ? 0U : 60U * 1000U;
}
