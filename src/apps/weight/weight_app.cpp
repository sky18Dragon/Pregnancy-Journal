#include "weight_app.h"

#include <cstdio>
#include <cstdlib>

#include "app_log.h"
#include "canvas.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sticky_app_lifecycle.h"
#include "sticky_display.h"
#include "sticky_rtc.h"
#include "sticky_touch.h"
#include "ui_language.h"
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

void box(int x, int y, int width, int height, bool filled = false) {
  if (filled)
    s_canvas->fill_rect(x, y, width, height, GrayLevel::Black);
  else
    s_canvas->draw_rect(x, y, width, height, GrayLevel::Black);
}

void button(int x, int y, int width, int height, const char *label,
            bool filled) {
  box(x, y, width, height, filled);
  const GrayLevel color = filled ? GrayLevel::White : GrayLevel::Black;
  const int text_x = x + (width - ui_text_width(label, 2U)) / 2;
  s_canvas->draw_text(text_x, y + (height - 14) / 2, ui_text(label), 2, color);
}

bool read_today() {
  StickyRtcDateTime value = {};
  if (sticky_rtc_read(value) != ESP_OK)
    return false;
  s_today = {value.year, value.month, value.day};
  return true;
}

void draw_trend() {
  constexpr int x = 356, y = 126, width = 410, height = 170;
  s_canvas->draw_line(x, y + height, x + width, y + height, GrayLevel::Black);
  s_canvas->draw_line(x, y, x, y + height, GrayLevel::Black);
  if (s_service.count() == 0U) {
    s_canvas->draw_text(x + 70, y + 70, ui_text("NO WEIGHT RECORDS"), 2,
                        GrayLevel::DarkGray);
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
  s_canvas->set_rotation(CanvasRotation::Deg0);
  s_canvas->clear(GrayLevel::White);
  s_canvas->draw_text(32, 24, ui_text("WEIGHT TRACKER"), 5, GrayLevel::Black);
  s_canvas->draw_text(32, 76, ui_text("PREGNANCY WEIGHT TREND"), 2,
                      GrayLevel::DarkGray);
  button(640, 24, 128, 48, s_settings.use_pounds ? "UNIT LB" : "UNIT KG",
         false);
  const WeightRecord *latest = s_service.latest();
  if (latest == nullptr) {
    s_canvas->draw_text(32, 142, "--.-", 8, GrayLevel::Black);
    s_canvas->draw_text(238, 180, "KG", 3, GrayLevel::Black);
  } else {
    char value[24] = {};
    const uint16_t shown = display_weight(latest->weight_tenths_kg);
    std::snprintf(value, sizeof(value), "%u.%u", shown / 10U, shown % 10U);
    s_canvas->draw_text(32, 142, value, 8, GrayLevel::Black);
    s_canvas->draw_text(238, 180, unit_name(), 3, GrayLevel::Black);
    WeightSummary summary = {};
    if (s_service.summary(s_settings.height_cm, s_settings.baseline_tenths_kg,
                          summary)) {
      char metrics[64] = {};
      const int16_t change = display_change(summary.change_tenths_kg);
      std::snprintf(metrics, sizeof(metrics), "BMI %u.%u   %+d.%d %s",
                    summary.bmi_tenths / 10U, summary.bmi_tenths % 10U,
                    change / 10, std::abs(change % 10), unit_name());
      s_canvas->draw_text(34, 244, metrics, 2, GrayLevel::Black);
      if (summary.outside_recommended_gain)
        s_canvas->draw_text(34, 280, ui_text("CHECK WEIGHT TREND"), 2,
                            GrayLevel::Black);
    }
  }
  draw_trend();
  s_canvas->draw_line(32, 330, 768, 330, GrayLevel::DarkGray);
  char profile[56] = {};
  const uint16_t baseline = display_weight(s_settings.baseline_tenths_kg);
  std::snprintf(profile, sizeof(profile), "HEIGHT %u CM   BASELINE %u.%u %s",
                s_settings.height_cm, baseline / 10U, baseline % 10U,
                unit_name());
  s_canvas->draw_text(34, 354, profile, 2, GrayLevel::Black);
  button(32, 400, 226, 56, "ADD TODAY", true);
  button(282, 400, 226, 56, "-  HEIGHT  +", false);
  button(532, 400, 236, 56, "REMOVE LATEST", false);
}

void render_editor() {
  s_canvas->clear(GrayLevel::White);
  s_canvas->draw_text(32, 24, ui_text("ADD WEIGHT"), 5, GrayLevel::Black);
  s_canvas->draw_text(32, 82, ui_text("TODAY'S WEIGHT"), 2,
                      GrayLevel::DarkGray);
  char value[24] = {};
  std::snprintf(value, sizeof(value), "%u.%u %s", s_weight / 10U,
                s_weight % 10U, unit_name());
  const int tx = (800 - ui_text_width(value, 8U)) / 2;
  s_canvas->draw_text(tx, 142, value, 8, GrayLevel::Black);
  button(40, 278, 160, 64, "- 1.0", false);
  button(216, 278, 160, 64, "- 0.1", false);
  button(424, 278, 160, 64, "+ 0.1", false);
  button(600, 278, 160, 64, "+ 1.0", false);
  button(40, 392, 232, 64, "CANCEL", false);
  button(528, 392, 232, 64, "SAVE", true);
}

void render(bool full = false) {
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
  if (s_editing) {
    if (y >= 278 && y < 342) {
      const uint16_t minimum = display_weight(300U);
      const uint16_t maximum = display_weight(2000U);
      if (x < 200 && s_weight >= minimum + 10U)
        s_weight -= 10U;
      else if (x < 400 && s_weight > minimum)
        --s_weight;
      else if (x < 600 && s_weight < maximum)
        ++s_weight;
      else if (x >= 600 && s_weight <= maximum - 10U)
        s_weight += 10U;
    } else if (y >= 392 && x < 272)
      s_editing = false;
    else if (y >= 392 && x >= 528)
      save();
    render();
    return;
  }
  if (x >= 640 && y >= 24 && y < 72) {
    s_settings.use_pounds = !s_settings.use_pounds;
    weight_storage_save(s_service, s_settings);
    render();
    return;
  }
  if (y < 400)
    return;
  if (x < 258) {
    read_today();
    const WeightRecord *today = s_service.find_date(s_today);
    const WeightRecord *latest = s_service.latest();
    const uint16_t stored = today != nullptr    ? today->weight_tenths_kg
                            : latest != nullptr ? latest->weight_tenths_kg
                                                : 600U;
    s_weight = display_weight(stored);
    s_editing = true;
  } else if (x < 508) {
    if (x < 395 && s_settings.height_cm > 100U)
      --s_settings.height_cm;
    else if (x >= 395 && s_settings.height_cm < 220U)
      ++s_settings.height_cm;
    weight_storage_save(s_service, s_settings);
  } else {
    const WeightRecord *latest = s_service.latest();
    if (latest != nullptr) {
      s_service.remove(latest->id);
      weight_storage_save(s_service, s_settings);
    }
  }
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
