#pragma once

#include <cstddef>
#include <cstdint>

#include "digit_fields.h"
#include "pregnancy_pages.h"
#include "pregnancy_state.h"

// Page flow of the Baby Week app, free of hardware: which page is shown, the
// clock / due-date entry, validation, and the progress shown on the dashboard.
// The app task owns the display, touch, RTC and storage and drives this class;
// the RTC is reached through PregnancyClockPort so host tests can fake it.
// 孕周应用的页面流程（页面切换、时间/日期输入与校验、进度计算），不接触硬件；
// 显示、触摸、RTC、存储由 App 任务负责，RTC 通过接口注入以便宿主测试。

struct PregnancyClockTime {
  uint16_t year = 0U;
  uint8_t month = 0U;
  uint8_t day = 0U;
  uint8_t hour = 0U;
  uint8_t minute = 0U;
  uint8_t second = 0U;
};

class PregnancyClockPort {
public:
  virtual ~PregnancyClockPort() = default;
  // False when the RTC is not ready or the read fails.
  virtual bool read(PregnancyClockTime &time) = 0;
  // False when the RTC rejects the write.
  virtual bool write(const PregnancyClockTime &time) = 0;
};

// What the app has to do after an action.
struct PregnancyFlowResult {
  bool render = false;          // redraw the (possibly new) page
  bool open_launcher = false;   // hand over to the launcher (no redraw)
  bool save_profile = false;    // start saving pending_profile() to storage
};

class PregnancyFlow {
public:
  explicit PregnancyFlow(PregnancyClockPort &clock) : clock_(clock) {}

  // Start-up: read the clock and pick the first page (clock setup without an
  // RTC, otherwise source setup until storage reports the saved profile).
  void begin();

  PregnancyFlowResult handle_action(PregnancyAction action);

  // Storage notifications.
  void unconfigure() { configured_ = false; }
  void profile_loaded(bool found, const PregnancyProfile &profile);
  void profile_saved();
  // After storage settles (not on the clock page): show the dashboard when a
  // profile exists, else (or when `setup_requested`) the source setup. Returns
  // true when the dashboard was entered.
  bool settle_after_storage(bool setup_requested);

  // Re-read the clock after midnight; true when the progress changed.
  bool refresh_for_new_day();

  PregnancyPage page() const { return page_; }
  bool configured() const { return configured_; }
  bool input_error() const { return input_error_; }
  bool due_date_primary() const { return due_date_primary_; }
  const DigitFields &clock_fields() const { return clock_fields_; }
  const DigitFields &date_fields() const { return date_fields_; }
  const PregnancyProfile &profile() const { return profile_; }
  const PregnancyProfile &pending_profile() const { return pending_profile_; }
  const PregnancyProgress &progress() const { return progress_; }
  bool showing_week_pages() const {
    return page_ == PregnancyPage::Dashboard || page_ == PregnancyPage::Baby ||
           page_ == PregnancyPage::Mom;
  }
  void clear_input_error() { input_error_ = false; }

private:
  void fill_clock_digits(const PregnancyClockTime &time);
  void fill_date_digits(const PregnancyDate &date);
  bool parse_clock(PregnancyClockTime &time) const;
  bool parse_profile_date(PregnancyDate &date) const;
  bool update_progress_from_clock();
  void open_clock_setup();
  void open_source_setup();
  void open_date_setup(bool due_date_primary);
  void apply_clock();
  PregnancyFlowResult apply_profile_date();

  PregnancyClockPort &clock_;
  PregnancyPage page_ = PregnancyPage::ClockSetup;
  PregnancyDate today_ = {};
  PregnancyProfile profile_ = {};
  PregnancyProfile pending_profile_ = {};
  PregnancyProgress progress_ = {};
  bool configured_ = false;
  bool due_date_primary_ = true;
  bool input_error_ = false;
  DigitFields clock_fields_{DigitFieldKind::DateTime};
  DigitFields date_fields_{DigitFieldKind::Date};
  int32_t last_day_index_ = -1;
};
