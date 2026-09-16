#include "weight_app.h"

#include <cstdio>
#include <cstdlib>

#include "app_log.h"
#include "canvas.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "prototype_ui.h"
#include "sticky_app.h"
#include "sticky_app_lifecycle.h"
#include "sticky_display.h"
#include "sticky_rtc.h"
#include "sticky_touch.h"
#include "ui_language.h"
#include "ui_tokens.h"
#include "weight_service.h"
#include "weight_storage.h"

namespace {
constexpr char kTag[] = "WEIGHT";
Canvas *s_canvas = nullptr;
TaskHandle_t s_task = nullptr;
StickyAppLifecycle s_lifecycle = {};
WeightService s_service;
WeightProfileSettings s_settings = {};
PregnancyDate s_today = {};
bool s_editing = false;
uint16_t s_weight = 600U;

bool portrait() { return s_canvas->height() > s_canvas->width(); }

prototype_ui::Rect unit_button_rect() {
  return portrait()
             ? prototype_ui::Rect{336, 88, 120,
                                  ui_tokens::kMinimumTouchHeight}
             : prototype_ui::Rect{548, 20, 92,
                                  ui_tokens::kMinimumTouchHeight};
}

prototype_ui::Rect dashboard_button_rect(size_t index) {
  if (portrait())
    return {24 + static_cast<int>(index) * 148, 720, 136, 56};
  return {24 + static_cast<int>(index) * 256, 400, 240, 56};
}

prototype_ui::Rect adjust_button_rect(size_t index) {
  if (portrait())
    return {24 + static_cast<int>(index % 2U) * 224,
            330 + static_cast<int>(index / 2U) * 72, 208, 56};
  return {56 + static_cast<int>(index) * 176, 260, 160, 48};
}

prototype_ui::Rect save_button_rect() {
  return portrait() ? prototype_ui::Rect{24, 720, 432, 56}
                    : prototype_ui::Rect{24, 400, 752, 56};
}

uint16_t display_weight(uint16_t tenths_kg) {
  return s_settings.use_pounds
             ? static_cast<uint16_t>(
                   (static_cast<uint32_t>(tenths_kg) * 22046U + 5000U) / 10000U)
             : tenths_kg;
}

int16_t display_change(int16_t tenths_kg) {
  return s_settings.use_pounds
             ? static_cast<int16_t>(static_cast<int32_t>(tenths_kg) * 22046L /
                                    10000L)
             : tenths_kg;
}

uint16_t stored_weight(uint16_t display_tenths) {
  return s_settings.use_pounds
             ? static_cast<uint16_t>(
                   (static_cast<uint32_t>(display_tenths) * 453592U + 500000U) /
                   1000000U)
             : display_tenths;
}

const char *unit_name() { return s_settings.use_pounds ? "LB" : "KG"; }

void button(int x, int y, int width, int height, const char *label,
            bool filled) {
  prototype_ui::draw_button(*s_canvas, {x, y, width, height}, label, filled, 2);
}

bool read_today() {
  StickyRtcDateTime value = {};
  if (sticky_rtc_read(value) != ESP_OK)
    return false;
  s_today = {value.year, value.month, value.day};
  return true;
}

void draw_trend() {
  const int x = portrait() ? 24 : 333;
  const int y = portrait() ? 330 : 116;
  const int width = portrait() ? 432 : 443;
  const int height = portrait() ? 238 : 186;
  s_canvas->draw_line(x, y + height, x + width, y + height, GrayLevel::Black);
  s_canvas->draw_line(x, y, x, y + height, GrayLevel::Black);
  if (s_service.count() == 0U) {
    s_canvas->draw_text(x + 70, y + 70, ui_text("NO WEIGHT RECORDS"), 2,
                        GrayLevel::Black);
    return;
  }
  const size_t start = s_service.count() > 7U ? s_service.count() - 7U : 0U;
  uint16_t minimum = 2000U, maximum = 0U;
  for (size_t i = start; i < s_service.count(); ++i) {
    const uint16_t value = s_service.at(i)->weight_tenths_kg;
    if (value < minimum)
      minimum = value;
    if (value > maximum)
      maximum = value;
  }
  if (maximum - minimum < 20U) {
    maximum += 10U;
    minimum -= minimum > 10U ? 10U : 0U;
  }
  const size_t points = s_service.count() - start;
  int previous_x = x, previous_y = y + height;
  for (size_t i = 0U; i < points; ++i) {
    const uint16_t value = s_service.at(start + i)->weight_tenths_kg;
    const int px = x + 16 +
                   static_cast<int>(i) * (width - 32) /
                       static_cast<int>(points > 1U ? points - 1U : 1U);
    const int py = y + height - 12 -
                   static_cast<int>(value - minimum) * (height - 24) /
                       static_cast<int>(maximum - minimum);
    s_canvas->fill_circle(px, py, 5, GrayLevel::Black);
    if (i > 0U)
      s_canvas->draw_line(previous_x, previous_y, px, py, GrayLevel::Black);
    previous_x = px;
    previous_y = py;
  }
}

void render_dashboard() {
  prototype_ui::begin_page(*s_canvas);
  prototype_ui::draw_header(*s_canvas, "WEIGHT TREND",
                            "FOCUS ON THE LONG-TERM TREND", "BACK");
  const prototype_ui::Rect unit = unit_button_rect();
  button(unit.x, unit.y, unit.width, unit.height,
         s_settings.use_pounds ? "LB / KG" : "KG / LB", false);
  const WeightRecord *latest = s_service.latest();
  if (latest == nullptr) {
    s_canvas->draw_text(24, portrait() ? 140 : 108, "--.-", 8,
                        GrayLevel::Black);
    s_canvas->draw_text(230, portrait() ? 182 : 150, "KG", 3, GrayLevel::Black);
  } else {
    char value[24] = {};
    const uint16_t shown = display_weight(latest->weight_tenths_kg);
    std::snprintf(value, sizeof(value), "%u.%u", shown / 10U, shown % 10U);
    s_canvas->draw_text(24, portrait() ? 140 : 108, value, 8, GrayLevel::Black);
    s_canvas->draw_text(230, portrait() ? 182 : 150, unit_name(), 3,
                        GrayLevel::Black);
    WeightSummary summary = {};
    if (s_service.summary(s_settings.height_cm, s_settings.baseline_tenths_kg,
                          summary)) {
      const int16_t change = display_change(summary.change_tenths_kg);
      char bmi[16] = {}, delta[24] = {};
      std::snprintf(bmi, sizeof(bmi), "%u.%u", summary.bmi_tenths / 10U,
                    summary.bmi_tenths % 10U);
      std::snprintf(delta, sizeof(delta), "%+d.%d %s", change / 10,
                    std::abs(change % 10), unit_name());
      const int summary_label_y = portrait() ? 242 : 204;
      const int summary_value_y = portrait() ? 264 : 224;
      s_canvas->draw_text(24, summary_label_y, "BMI", 1, GrayLevel::Black);
      s_canvas->draw_text(24, summary_value_y, bmi, 3, GrayLevel::Black);
      s_canvas->draw_text(124, summary_label_y, ui_text("FROM BASELINE"), 1,
                          GrayLevel::Black);
      s_canvas->draw_text(124, summary_value_y, delta, 3, GrayLevel::Black);
    }
  }
  s_canvas->draw_text(portrait() ? 24 : 333, portrait() ? 300 : 90,
                      ui_text("LATEST 7 RECORDS"), 2, GrayLevel::Black);
  draw_trend();
  const int divider_y = portrait() ? 600 : 368;
  s_canvas->draw_line(24, divider_y, s_canvas->width() - 24, divider_y,
                      GrayLevel::Black);
  char profile[80] = {};
  const uint16_t baseline = display_weight(s_settings.baseline_tenths_kg);
  std::snprintf(profile, sizeof(profile),
                ui_language_is_chinese()
                    ? "身高 %u CM     基线 %u.%u %s     记录 %u 次"
                    : "HEIGHT %u CM     BASELINE %u.%u %s     RECORDS %u",
                s_settings.height_cm, baseline / 10U, baseline % 10U,
                unit_name(), static_cast<unsigned>(s_service.count()));
  s_canvas->draw_text(24, portrait() ? 620 : 380, profile, 1, GrayLevel::Black);
  const char *labels[] = {"ADD TODAY", "HEIGHT", "REMOVE LATEST"};
  for (size_t i = 0U; i < 3U; ++i) {
    const prototype_ui::Rect rect = dashboard_button_rect(i);
    button(rect.x, rect.y, rect.width, rect.height, labels[i], i == 0U);
  }
}

void render_editor() {
  prototype_ui::begin_page(*s_canvas);
  prototype_ui::draw_header(*s_canvas, "RECORD TODAY'S WEIGHT",
                            "SAVING AGAIN REPLACES TODAY'S RECORD", "CANCEL");
  char value[24] = {};
  std::snprintf(value, sizeof(value), "%u.%u", s_weight / 10U, s_weight % 10U);
  const int tx = (s_canvas->width() - ui_text_width(value, 10U)) / 2;
  s_canvas->draw_text(tx, portrait() ? 150 : 112, value, 10, GrayLevel::Black);
  const int ux = (s_canvas->width() - ui_text_width(unit_name(), 3U)) / 2;
  s_canvas->draw_text(ux, portrait() ? 238 : 198, unit_name(), 3,
                      GrayLevel::Black);
  const char *labels[] = {"- 1.0", "- 0.1", "+ 0.1", "+ 1.0"};
  for (size_t i = 0U; i < 4U; ++i) {
    const prototype_ui::Rect rect = adjust_button_rect(i);
    button(rect.x, rect.y, rect.width, rect.height, labels[i], false);
  }
  const prototype_ui::Rect save = save_button_rect();
  button(save.x, save.y, save.width, save.height, "SAVE TODAY", true);
}

void render(bool full = false) {
  sticky_display_set_battery_overlay_enabled(false);
  if (s_editing)
    render_editor();
  else
    render_dashboard();
  if (full)
    sticky_display_refresh_monochrome();
  else
    sticky_display_refresh_partial();
}

void save() {
  WeightRecord record = {};
  record.date = s_today;
  record.weight_tenths_kg = stored_weight(s_weight);
  if (s_settings.baseline_tenths_kg == 0U)
    s_settings.baseline_tenths_kg = record.weight_tenths_kg;
  if (s_service.upsert(record))
    weight_storage_save(s_service, s_settings);
  s_editing = false;
}

void handle(int x, int y) {
  const prototype_ui::Rect back =
      prototype_ui::header_action_rect(s_canvas->width(), s_canvas->height());
  if (s_editing) {
    if (back.contains(x, y)) {
      s_editing = false;
    } else {
      const uint16_t minimum = display_weight(300U);
      const uint16_t maximum = display_weight(2000U);
      if (adjust_button_rect(0U).contains(x, y) && s_weight >= minimum + 10U)
        s_weight -= 10U;
      else if (adjust_button_rect(1U).contains(x, y) && s_weight > minimum)
        --s_weight;
      else if (adjust_button_rect(2U).contains(x, y) && s_weight < maximum)
        ++s_weight;
      else if (adjust_button_rect(3U).contains(x, y) &&
               s_weight <= maximum - 10U)
        s_weight += 10U;
      else if (save_button_rect().contains(x, y))
        save();
    }
    render();
    return;
  }
  if (back.contains(x, y)) {
    sticky_app_request_launcher();
    return;
  }
  if (unit_button_rect().contains(x, y)) {
    s_settings.use_pounds = !s_settings.use_pounds;
    weight_storage_save(s_service, s_settings);
    render();
    return;
  }
  if (dashboard_button_rect(0U).contains(x, y)) {
    read_today();
    const WeightRecord *today = s_service.find_date(s_today);
    const WeightRecord *latest = s_service.latest();
    const uint16_t stored = today != nullptr    ? today->weight_tenths_kg
                            : latest != nullptr ? latest->weight_tenths_kg
                                                : 600U;
    s_weight = display_weight(stored);
    s_editing = true;
  } else if (dashboard_button_rect(1U).contains(x, y)) {
    const prototype_ui::Rect height = dashboard_button_rect(1U);
    if (x < height.x + height.width / 2 && s_settings.height_cm > 100U)
      --s_settings.height_cm;
    else if (x >= height.x + height.width / 2 && s_settings.height_cm < 220U)
      ++s_settings.height_cm;
    weight_storage_save(s_service, s_settings);
  } else if (dashboard_button_rect(2U).contains(x, y)) {
    const WeightRecord *latest = s_service.latest();
    if (latest != nullptr) {
      s_service.remove(latest->id);
      weight_storage_save(s_service, s_settings);
    }
  } else
    return;
  render();
}

void task(void *) {
  weight_storage_load(s_service, s_settings);
  read_today();
  render(true);
  while (true) {
    if (sticky_app_lifecycle_checkpoint(s_lifecycle)) {
      weight_storage_load(s_service, s_settings);
      read_today();
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

esp_err_t weight_app_start(Canvas &canvas) {
  app_log_register_tag(kTag);
  if (s_task != nullptr)
    return ESP_OK;
  s_canvas = &canvas;
  return xTaskCreate(task, "weight", 5120U, nullptr, 3U, &s_task) == pdPASS
             ? ESP_OK
             : ESP_ERR_NO_MEM;
}
esp_err_t weight_app_pause() {
  return sticky_app_lifecycle_pause(s_lifecycle, s_task);
}
esp_err_t weight_app_resume() {
  if (s_task == nullptr)
    return ESP_ERR_INVALID_STATE;
  sticky_app_lifecycle_resume(s_lifecycle);
  return ESP_OK;
}
esp_err_t weight_app_prepare_power_sleep(uint32_t &current, uint32_t &next) {
  current = 0U;
  next = 0U;
  return weight_app_pause();
}
uint32_t weight_app_power_sleep_timeout_ms() {
  return s_editing ? 0U : 60U * 1000U;
}
