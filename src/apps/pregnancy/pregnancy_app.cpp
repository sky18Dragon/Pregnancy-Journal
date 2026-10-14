#include "pregnancy_app.h"

#include <atomic>
#include <cstdio>

#include "app_log.h"
#include "battery_status_overlay.h"
#include "content_service.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pregnancy_pages.h"
#include "pregnancy_repository_store.h"
#include "pregnancy_state.h"
#include "press_feedback.h"
#include "prototype_ui.h"
#include "refresh_policy.h"
#include "sticky_app.h"
#include "sticky_app_lifecycle.h"
#include "sticky_display.h"
#include "sticky_rtc.h"
#include "sticky_touch.h"

namespace {

constexpr char kTag[] = "PREGNANCY";
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(40);
constexpr uint32_t kTaskStackSize = 5120U;
constexpr UBaseType_t kTaskPriority = 3U;
constexpr uint32_t kDashboardSleepTimeoutMs = 60U * 1000U;
constexpr int64_t kRtcPollIntervalUs = 60LL * 1000000LL;

Canvas *s_canvas = nullptr;
TaskHandle_t s_app_task = nullptr;
StickyAppLifecycle s_lifecycle = {};
CanvasRotation s_display_rotation = CanvasRotation::Deg0;
PregnancyPage s_page = PregnancyPage::ClockSetup;
PageRefreshTracker s_refresh;
constexpr int kStatusPageId = 1000;
press_feedback::Pending s_press;

// Draws the tapped target pressed and shows it once, before a page change
// whose full refresh would otherwise leave the panel unchanged for ~2 s.
void show_press_feedback() {
  press_feedback::Rect rect = {};
  if (!s_press.take(rect)) return;
  press_feedback::draw(*s_canvas, rect);
  sticky_display_refresh_partial();
}
PregnancyDate s_today = {};
PregnancyProfile s_profile = {};
PregnancyProgress s_progress = {};
bool s_configured = false;
bool s_due_date_primary = true;
bool s_input_error = false;
bool s_replace_on_digit = false;
char s_clock_digits[13] = {};
size_t s_clock_length = 0U;
char s_date_digits[9] = {};
size_t s_date_length = 0U;
int32_t s_last_day_index = -1;
int64_t s_next_rtc_poll_us = 0;
std::atomic<bool> s_setup_requested{false};
pregnancy_repository::PregnancyRepositoryStore &s_profile_store =
    pregnancy_repository::pregnancy_repository_store();
PregnancyProfile s_pending_profile = {};
bool s_profile_loading = false;
bool s_profile_saving = false;
bool s_retry_save = false;
bool s_reload_after_pending = false;
bool s_setup_after_load = false;
sticky_storage::SdRepositoryAdapterStatus s_storage_error =
    sticky_storage::SdRepositoryAdapterStatus::Idle;

prototype_ui::Rect storage_retry_rect() {
  return s_canvas->height() > s_canvas->width()
             ? prototype_ui::Rect{24, 720, 432, 56}
             : prototype_ui::Rect{24, 400, 752, 56};
}

const char *storage_error_copy(
    sticky_storage::SdRepositoryAdapterStatus status) {
  using sticky_storage::SdRepositoryAdapterStatus;
  switch (status) {
  case SdRepositoryAdapterStatus::CardMissing:
    return "MICROSD CARD IS REQUIRED";
  case SdRepositoryAdapterStatus::CardReadOnly:
    return "MICROSD CARD IS READ ONLY";
  case SdRepositoryAdapterStatus::CardFull:
  case SdRepositoryAdapterStatus::CapacityReached:
    return "MICROSD STORAGE IS FULL";
  case SdRepositoryAdapterStatus::DatasetUnavailable:
    return "CREATE OR SELECT A DATASET IN SETTINGS";
  case SdRepositoryAdapterStatus::RecoveryUnavailable:
  case SdRepositoryAdapterStatus::Corrupt:
    return "MICROSD DATA NEEDS REVIEW";
  case SdRepositoryAdapterStatus::StaleGeneration:
    return "MICROSD CARD CHANGED";
  default:
    return "MICROSD STORAGE UNAVAILABLE";
  }
}

void render_storage_state() {
  prototype_ui::begin_page(*s_canvas);
  prototype_ui::draw_header(*s_canvas, "MICROSD DATA",
                            "PREGNANCY PROFILE STORAGE", "BACK");
  const bool portrait = s_canvas->height() > s_canvas->width();
  if (s_profile_loading || s_profile_saving) {
    s_canvas->draw_text(32, portrait ? 260 : 176,
                        s_profile_saving ? "SAVING TO MICROSD"
                                         : "LOADING FROM MICROSD",
                        4, GrayLevel::Black);
    return;
  }
  s_canvas->draw_text(32, portrait ? 226 : 154,
                      storage_error_copy(s_storage_error), 3,
                      GrayLevel::Black);
  s_canvas->draw_text(32, portrait ? 286 : 210,
                      "NO FLASH FALLBACK WAS USED", 2,
                      GrayLevel::Black);
  prototype_ui::draw_button(*s_canvas, storage_retry_rect(), "RETRY", true, 2);
}

void fill_clock_digits(const StickyRtcDateTime &value) {
  const uint16_t year = static_cast<uint16_t>(value.year % 10000U);
  s_clock_digits[0] = static_cast<char>('0' + year / 1000U);
  s_clock_digits[1] = static_cast<char>('0' + year / 100U % 10U);
  s_clock_digits[2] = static_cast<char>('0' + year / 10U % 10U);
  s_clock_digits[3] = static_cast<char>('0' + year % 10U);
  const uint8_t fields[] = {
      value.month,
      value.day,
      value.hour,
      value.minute,
  };
  for (size_t index = 0U; index < 4U; ++index) {
    s_clock_digits[4U + index * 2U] =
        static_cast<char>('0' + fields[index] / 10U);
    s_clock_digits[5U + index * 2U] =
        static_cast<char>('0' + fields[index] % 10U);
  }
  s_clock_length = 12U;
  s_clock_digits[s_clock_length] = '\0';
}

void fill_date_digits(const PregnancyDate &value) {
  const uint16_t year = static_cast<uint16_t>(value.year % 10000U);
  s_date_digits[0] = static_cast<char>('0' + year / 1000U);
  s_date_digits[1] = static_cast<char>('0' + year / 100U % 10U);
  s_date_digits[2] = static_cast<char>('0' + year / 10U % 10U);
  s_date_digits[3] = static_cast<char>('0' + year % 10U);
  s_date_digits[4] = static_cast<char>('0' + value.month / 10U);
  s_date_digits[5] = static_cast<char>('0' + value.month % 10U);
  s_date_digits[6] = static_cast<char>('0' + value.day / 10U);
  s_date_digits[7] = static_cast<char>('0' + value.day % 10U);
  s_date_length = 8U;
  s_date_digits[s_date_length] = '\0';
}

uint16_t digits_u16(const char *digits, size_t offset, size_t count) {
  uint16_t value = 0U;
  for (size_t index = 0U; index < count; ++index) {
    value = static_cast<uint16_t>(
        value * 10U + static_cast<uint16_t>(digits[offset + index] - '0'));
  }
  return value;
}

bool parse_clock(StickyRtcDateTime &value) {
  if (s_clock_length != 12U)
    return false;
  value.year = digits_u16(s_clock_digits, 0U, 4U);
  value.month = static_cast<uint8_t>(digits_u16(s_clock_digits, 4U, 2U));
  value.day = static_cast<uint8_t>(digits_u16(s_clock_digits, 6U, 2U));
  value.hour = static_cast<uint8_t>(digits_u16(s_clock_digits, 8U, 2U));
  value.minute = static_cast<uint8_t>(digits_u16(s_clock_digits, 10U, 2U));
  value.second = 0U;
  return pregnancy_date_valid({value.year, value.month, value.day}) &&
         value.hour <= 23U && value.minute <= 59U;
}

bool parse_profile_date(PregnancyDate &value) {
  if (s_date_length != 8U)
    return false;
  value.year = digits_u16(s_date_digits, 0U, 4U);
  value.month = static_cast<uint8_t>(digits_u16(s_date_digits, 4U, 2U));
  value.day = static_cast<uint8_t>(digits_u16(s_date_digits, 6U, 2U));
  return pregnancy_date_valid(value);
}

bool read_rtc(StickyRtcDateTime &value) {
  return sticky_rtc_is_ready() && sticky_rtc_read(value) == ESP_OK;
}

bool update_progress_from_clock() {
  StickyRtcDateTime clock = {};
  if (!read_rtc(clock))
    return false;
  s_today = {clock.year, clock.month, clock.day};
  if (!pregnancy_date_to_day_index(s_today, s_last_day_index))
    return false;
  fill_clock_digits(clock);
  return s_configured &&
         pregnancy_progress_calculate(s_today, s_profile, s_progress);
}

// Page changes always use the full waveform; `partial` only applies when the
// page on screen stays the same (see PageRefreshTracker).
void refresh_display(bool partial, int page_id) {
  const bool full = s_refresh.needs_full(page_id, !partial);
  const esp_err_t result = full ? sticky_display_refresh_monochrome()
                                : sticky_display_refresh_partial();
  if (result != ESP_OK) {
    STICKY_LOGE(kTag, "display=refresh mode=%s result=%s",
                full ? "full" : "partial", esp_err_to_name(result));
  }
}

bool status_page_active() {
  return s_profile_loading || s_profile_saving ||
         (s_storage_error != sticky_storage::SdRepositoryAdapterStatus::Idle &&
          s_storage_error != sticky_storage::SdRepositoryAdapterStatus::Ok);
}

void render_page(bool partial) {
  sticky_display_set_battery_overlay_enabled(false);
  s_canvas->set_rotation(s_display_rotation);
  const bool status = status_page_active();
  if (s_refresh.page_changes(status ? kStatusPageId : static_cast<int>(s_page)))
    show_press_feedback();
  else
    s_press.clear();
  if (status) {
    render_storage_state();
    refresh_display(partial, kStatusPageId);
    return;
  }
  switch (s_page) {
  case PregnancyPage::ClockSetup:
    pregnancy_page_render_clock_setup(*s_canvas, s_clock_digits, s_input_error,
                                      s_configured);
    break;
  case PregnancyPage::SourceSetup:
    pregnancy_page_render_source_setup(*s_canvas, s_configured);
    break;
  case PregnancyPage::DateSetup:
    pregnancy_page_render_profile_date_setup(*s_canvas, s_date_digits,
                                             s_input_error, s_due_date_primary);
    break;
  case PregnancyPage::Dashboard: {
    pregnancy_page_render_dashboard(*s_canvas, s_profile.estimated_due_date,
                                    s_progress);
    if (s_canvas->width() > s_canvas->height())
      battery_status_overlay_draw_compact(*s_canvas, 620, 29);
  } break;
  case PregnancyPage::Baby:
  case PregnancyPage::Mom:
    pregnancy_page_render_detail(*s_canvas, s_page, s_progress,
                                 pregnancy_content_for_week(s_progress.weeks));
    break;
  }
  refresh_display(partial, static_cast<int>(s_page));
}

void open_clock_setup() {
  StickyRtcDateTime clock = {};
  if (read_rtc(clock))
    fill_clock_digits(clock);
  else {
    s_clock_digits[0] = '\0';
    s_clock_length = 0U;
  }
  s_page = PregnancyPage::ClockSetup;
  s_input_error = false;
  s_replace_on_digit = s_clock_length > 0U;
}

void open_source_setup() {
  s_page = PregnancyPage::SourceSetup;
  s_input_error = false;
  s_replace_on_digit = false;
}

void open_date_setup(bool due_date_primary) {
  s_due_date_primary = due_date_primary;
  if (s_configured) {
    fill_date_digits(due_date_primary ? s_profile.estimated_due_date
                                      : s_profile.last_menstrual_period);
  } else {
    s_date_digits[0] = '\0';
    s_date_length = 0U;
  }
  s_page = PregnancyPage::DateSetup;
  s_input_error = false;
  s_replace_on_digit = s_date_length > 0U;
}

void append_digit(char *buffer, size_t &length, size_t maximum, char digit) {
  if (s_replace_on_digit) {
    length = 0U;
    buffer[0] = '\0';
    s_replace_on_digit = false;
  }
  if (length < maximum) {
    buffer[length++] = digit;
    buffer[length] = '\0';
  }
}

void delete_digit(char *buffer, size_t &length) {
  if (s_replace_on_digit) {
    length = 0U;
    buffer[0] = '\0';
    s_replace_on_digit = false;
  } else if (length > 0U) {
    buffer[--length] = '\0';
  }
}

bool apply_clock() {
  StickyRtcDateTime value = {};
  if (!parse_clock(value) || sticky_rtc_write(value) != ESP_OK) {
    s_input_error = true;
    return true;
  }
  s_today = {value.year, value.month, value.day};
  pregnancy_date_to_day_index(s_today, s_last_day_index);
  if (s_configured &&
      pregnancy_progress_calculate(s_today, s_profile, s_progress)) {
    s_page = PregnancyPage::Dashboard;
  } else {
    open_source_setup();
  }
  return true;
}

bool apply_profile_date() {
  PregnancyDate value = {};
  PregnancyProfile profile = {};
  if (!parse_profile_date(value) ||
      !(s_due_date_primary ? pregnancy_profile_from_due_date(value, profile)
                           : pregnancy_profile_from_lmp(value, profile)) ||
      !pregnancy_progress_calculate(s_today, profile, s_progress)) {
    s_input_error = true;
    return true;
  }
  s_pending_profile = profile;
  s_retry_save = true;
  s_storage_error = s_profile_store.begin_save(profile);
  s_profile_saving =
      s_storage_error == sticky_storage::SdRepositoryAdapterStatus::Pending;
  if (s_profile_saving) s_storage_error =
      sticky_storage::SdRepositoryAdapterStatus::Idle;
  return true;
}

bool handle_action(PregnancyAction action) {
  char digit = '\0';
  if (pregnancy_action_digit(action, digit)) {
    s_input_error = false;
    if (s_page == PregnancyPage::ClockSetup)
      append_digit(s_clock_digits, s_clock_length, 12U, digit);
    else if (s_page == PregnancyPage::DateSetup)
      append_digit(s_date_digits, s_date_length, 8U, digit);
    return true;
  }
  switch (action) {
  case PregnancyAction::Delete:
    s_input_error = false;
    if (s_page == PregnancyPage::ClockSetup)
      delete_digit(s_clock_digits, s_clock_length);
    else if (s_page == PregnancyPage::DateSetup)
      delete_digit(s_date_digits, s_date_length);
    return true;
  case PregnancyAction::Continue:
    return s_page == PregnancyPage::ClockSetup ? apply_clock()
                                               : apply_profile_date();
  case PregnancyAction::SelectDueDate:
    open_date_setup(true);
    return true;
  case PregnancyAction::SelectLmp:
    open_date_setup(false);
    return true;
  case PregnancyAction::ShowOverview:
    s_page = PregnancyPage::Dashboard;
    return true;
  case PregnancyAction::ShowBaby:
    s_page = PregnancyPage::Baby;
    return true;
  case PregnancyAction::ShowMom:
    s_page = PregnancyPage::Mom;
    return true;
  case PregnancyAction::Back:
    if (s_page == PregnancyPage::DateSetup)
      open_source_setup();
    else if (s_configured) {
      update_progress_from_clock();
      s_page = PregnancyPage::Dashboard;
    }
    return true;
  case PregnancyAction::Edit:
    open_source_setup();
    return true;
  case PregnancyAction::OpenLauncher:
    show_press_feedback();
    sticky_app_request_launcher();
    return false;
  default:
    return false;
  }
}

PregnancyAction action_for_press(const StickyTouchPress &press) {
  int x = 0, y = 0;
  s_canvas->physical_to_logical(press.x, press.y, x, y);
  return pregnancy_page_action_at(s_page, s_configured, s_canvas->width(),
                                  s_canvas->height(), x, y);
}

void initialize_state() {
  StickyRtcDateTime clock = {};
  if (!read_rtc(clock)) {
    open_clock_setup();
  } else {
    fill_clock_digits(clock);
    s_today = {clock.year, clock.month, clock.day};
    pregnancy_date_to_day_index(s_today, s_last_day_index);
    open_source_setup();
  }
  s_configured = false;
  s_retry_save = false;
  s_storage_error = s_profile_store.begin_load();
  s_profile_loading =
      s_storage_error == sticky_storage::SdRepositoryAdapterStatus::Pending;
  if (s_profile_loading) {
    s_storage_error = sticky_storage::SdRepositoryAdapterStatus::Idle;
  }
}

bool poll_profile_storage() {
  if (!s_profile_loading && !s_profile_saving) return false;
  const sticky_storage::SdRepositoryAdapterStatus status =
      s_profile_store.poll();
  if (status == sticky_storage::SdRepositoryAdapterStatus::Pending)
    return false;
  const bool was_loading = s_profile_loading;
  s_profile_loading = false;
  s_profile_saving = false;
  if (status != sticky_storage::SdRepositoryAdapterStatus::Ok) {
    s_storage_error = status;
  } else {
    s_storage_error = sticky_storage::SdRepositoryAdapterStatus::Idle;
    if (was_loading) {
      s_configured = s_profile_store.found();
      if (s_configured) s_profile = s_profile_store.profile();
    } else {
      s_profile = s_pending_profile;
      s_configured = true;
      s_retry_save = false;
      s_input_error = false;
      STICKY_LOGI(kTag, "profile=save backend=microsd result=ok");
    }
  }
  if (s_reload_after_pending) {
    s_reload_after_pending = false;
    s_retry_save = false;
    s_storage_error = s_profile_store.begin_load();
    s_profile_loading =
        s_storage_error == sticky_storage::SdRepositoryAdapterStatus::Pending;
    if (s_profile_loading)
      s_storage_error = sticky_storage::SdRepositoryAdapterStatus::Idle;
    return true;
  }
  if (status != sticky_storage::SdRepositoryAdapterStatus::Ok) {
    // A queued write may have reached durable storage before an I/O error was
    // reported. Reload on retry instead of blindly replaying the old revision.
    s_retry_save = false;
    return true;
  }
  if (s_page != PregnancyPage::ClockSetup) {
    if (s_setup_after_load) {
      s_setup_after_load = false;
      open_source_setup();
    } else if (s_configured &&
        pregnancy_progress_calculate(s_today, s_profile, s_progress)) {
      s_page = PregnancyPage::Dashboard;
      s_next_rtc_poll_us = esp_timer_get_time() + kRtcPollIntervalUs;
    } else {
      open_source_setup();
    }
  }
  return true;
}

void retry_profile_storage() {
  s_storage_error = s_retry_save
                        ? s_profile_store.begin_save(s_pending_profile)
                        : s_profile_store.begin_load();
  s_profile_saving = s_retry_save &&
      s_storage_error == sticky_storage::SdRepositoryAdapterStatus::Pending;
  s_profile_loading = !s_retry_save &&
      s_storage_error == sticky_storage::SdRepositoryAdapterStatus::Pending;
  if (s_profile_loading || s_profile_saving)
    s_storage_error = sticky_storage::SdRepositoryAdapterStatus::Idle;
}

void update_for_date_rollover() {
  if ((s_page != PregnancyPage::Dashboard && s_page != PregnancyPage::Baby &&
       s_page != PregnancyPage::Mom) ||
      esp_timer_get_time() < s_next_rtc_poll_us)
    return;
  s_next_rtc_poll_us = esp_timer_get_time() + kRtcPollIntervalUs;
  StickyRtcDateTime clock = {};
  if (!read_rtc(clock))
    return;
  const PregnancyDate today = {clock.year, clock.month, clock.day};
  int32_t day_index = 0;
  if (!pregnancy_date_to_day_index(today, day_index) ||
      day_index == s_last_day_index)
    return;
  s_today = today;
  s_last_day_index = day_index;
  if (pregnancy_progress_calculate(today, s_profile, s_progress)) {
    render_page(true);
    STICKY_LOGI(kTag, "date=rollover week=%d day=%u result=updated",
                static_cast<int>(s_progress.weeks),
                static_cast<unsigned>(s_progress.days));
  }
}

void app_task(void *) {
  sticky_touch_clear_press();
  initialize_state();
  render_page(false);
  while (true) {
    if (poll_profile_storage()) render_page(false);
    if (!s_profile_store.pending() &&
        s_profile_store.status() == sticky_storage::SdRepositoryAdapterStatus::Ok &&
        !s_profile_store.ready()) {
      s_configured = false;
      s_retry_save = false;
      s_storage_error = s_profile_store.begin_load();
      s_profile_loading = s_storage_error == sticky_storage::SdRepositoryAdapterStatus::Pending;
      if (s_profile_loading) s_storage_error = sticky_storage::SdRepositoryAdapterStatus::Idle;
      render_page(true);
    }
    if (sticky_app_lifecycle_checkpoint(s_lifecycle)) {
      sticky_touch_clear_press();
      s_setup_after_load = s_setup_after_load || s_setup_requested.exchange(false);
      if (s_profile_store.pending()) {
        s_reload_after_pending = true;
      } else {
        s_retry_save = false;
        s_storage_error = s_profile_store.begin_load();
        s_profile_loading =
            s_storage_error ==
            sticky_storage::SdRepositoryAdapterStatus::Pending;
        if (s_profile_loading)
          s_storage_error = sticky_storage::SdRepositoryAdapterStatus::Idle;
      }
      render_page(false);
    }
    StickyTouchPress press = {};
    if (sticky_touch_take_press(press)) {
      if (s_profile_loading || s_profile_saving) {
        sticky_touch_clear_press();
        vTaskDelay(kPollInterval);
        continue;
      }
      if (s_storage_error !=
              sticky_storage::SdRepositoryAdapterStatus::Idle &&
          s_storage_error != sticky_storage::SdRepositoryAdapterStatus::Ok) {
        int x = 0, y = 0;
        s_canvas->physical_to_logical(press.x, press.y, x, y);
        if (prototype_ui::header_action_rect(
                s_canvas->width(), s_canvas->height()).contains(x, y)) {
          s_press.set(prototype_ui::header_action_rect(s_canvas->width(),
                                                       s_canvas->height()));
          show_press_feedback();
          sticky_app_request_launcher();
        } else if (storage_retry_rect().contains(x, y)) {
          s_press.set(storage_retry_rect());
          retry_profile_storage();
          render_page(true);
        }
        sticky_touch_clear_press();
        continue;
      }
      const PregnancyAction action = action_for_press(press);
      {
        // The tapped target comes from the same table the renderer drew.
        int x = 0, y = 0;
        s_canvas->physical_to_logical(press.x, press.y, x, y);
        PregnancyTarget targets[16];
        const size_t count = pregnancy_page_targets(
            s_page, s_configured, s_canvas->width(), s_canvas->height(),
            targets, sizeof(targets) / sizeof(targets[0]));
        for (size_t index = 0U; index < count; ++index) {
          if (targets[index].rect.contains(x, y)) {
            s_press.set(targets[index].rect);
            break;
          }
        }
      }
      if (handle_action(action)) {
        sticky_touch_clear_press();
        render_page(true);
      }
      s_press.clear();
    }
    update_for_date_rollover();
    vTaskDelay(kPollInterval);
  }
}

} // namespace

esp_err_t pregnancy_app_start(Canvas &canvas) {
  app_log_register_tag(kTag);
  if (s_app_task != nullptr)
    return ESP_OK;
  s_canvas = &canvas;
  return xTaskCreate(app_task, "pregnancy_app", kTaskStackSize, nullptr,
                     kTaskPriority, &s_app_task) == pdPASS
             ? ESP_OK
             : ESP_ERR_NO_MEM;
}

void pregnancy_app_set_display_rotation(CanvasRotation rotation) {
  s_display_rotation = rotation;
}

void pregnancy_app_request_setup() { s_setup_requested.store(true); }

esp_err_t pregnancy_app_pause() {
  return sticky_app_lifecycle_pause(s_lifecycle, s_app_task);
}

esp_err_t pregnancy_app_resume() {
  if (s_app_task == nullptr)
    return ESP_ERR_INVALID_STATE;
  sticky_app_lifecycle_resume(s_lifecycle);
  return ESP_OK;
}

esp_err_t pregnancy_app_prepare_power_sleep(uint32_t &current, uint32_t &next) {
  current = 0U;
  next = 0U;
  const esp_err_t paused = pregnancy_app_pause();
  if (paused != ESP_OK)
    return paused;
  StickyRtcDateTime clock = {};
  if (!read_rtc(clock))
    return ESP_OK;
  const PregnancyDate today = {clock.year, clock.month, clock.day};
  int32_t day_index = 0;
  if (!pregnancy_date_to_day_index(today, day_index))
    return ESP_OK;
  current = static_cast<uint32_t>(day_index) * 86400U +
            static_cast<uint32_t>(clock.hour) * 3600U +
            static_cast<uint32_t>(clock.minute) * 60U + clock.second;
  next = static_cast<uint32_t>(day_index + 1) * 86400U + 3U * 3600U;
  return ESP_OK;
}

uint32_t pregnancy_app_power_sleep_timeout_ms() {
  return s_page == PregnancyPage::Dashboard || s_page == PregnancyPage::Baby ||
                 s_page == PregnancyPage::Mom
             ? kDashboardSleepTimeoutMs
             : 0U;
}
