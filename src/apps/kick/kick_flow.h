#pragma once

#include <cstddef>
#include <cstdint>

#include "kick.h"
#include "kick_counter.h"
#include "kick_pages.h"

// Page and session flow of the Kicks app, free of hardware: which page is
// shown, the running counter, which record to save, and where Back goes. The
// app task owns the display, touch, RTC and the repository; it drives this
// class and carries out what the result asks for. The RTC and the record
// capacity are reached through KickFlowPort so host tests can fake them.
// 胎动应用的页面与计数流程（页面切换、计数器、要保存的记录、返回去向），不接触硬件。

struct KickClockTime {
  PregnancyDate date = {};
  uint8_t hour = 0U;
  uint8_t minute = 0U;
};

class KickFlowPort {
public:
  virtual ~KickFlowPort() = default;
  // False when the RTC cannot be read.
  virtual bool read_clock(KickClockTime &time) = 0;
  // True when no more records can be stored.
  virtual bool at_capacity() = 0;
  // The record method the user picked last; false when none was picked yet.
  virtual bool remembered_mode(KickMode &mode) = 0;
};

// What the app has to do after an action.
struct KickFlowResult {
  bool render = false;           // redraw the (possibly new) page
  bool open_launcher = false;    // hand over to the launcher (no redraw)
  bool capacity_reached = false; // report "record limit reached" and redraw
  bool save_session = false;     // save `session` (the app assigns its id)
  KickSession session = {};
  bool remember_mode = false;    // the user picked `mode`: remember it
  KickMode mode = KickMode::Timed;
};

class KickFlow {
public:
  explicit KickFlow(KickFlowPort &port) : port_(port) {}

  KickFlowResult handle_action(KickAction action, int64_t now_us);

  // The double-click shortcut: start recording like tapping "start counting"
  // (remembered method, record limit and clock checks included). A running
  // counter is never replaced; the result then asks for no redraw.
  // 双击快捷键：等同点“开始计数”；进行中的计数不会被替换。
  KickFlowResult enter_start(int64_t now_us);

  // The counter reached its time limit: finish it (save, or drop an empty one).
  bool counter_expired(int64_t now_us) const {
    return page_ == KickPage::Counter && counter_.expired(now_us);
  }
  KickFlowResult finish(int64_t now_us);

  // Clock: `refresh_clock` re-reads the RTC and reports a date change.
  bool read_clock();
  bool refresh_clock();

  // Storage notifications.
  void go_today() { page_ = KickPage::Today; }
  void abandon_session() {
    counter_.cancel();
    page_ = KickPage::Today;
  }
  void mark_save_failed() { save_failed_ = true; }
  void session_saved(const KickSession &session);

  KickPage page() const { return page_; }
  KickPeriod period() const { return period_; }
  const KickCounter &counter() const { return counter_; }
  uint16_t elapsed_minutes(int64_t now_us) const {
    return counter_.elapsed_minutes(now_us);
  }
  const PregnancyDate &today() const { return today_; }
  bool save_failed() const { return save_failed_; }
  const KickSession &result_session() const { return result_session_; }

private:
  KickPeriod inferred_period() const;
  KickFlowResult save_feel(KickFeel feel);
  KickFlowResult begin_counter(KickMode mode, int64_t now_us);
  KickFlowResult start_with(KickMode mode, int64_t now_us);
  KickFlowResult chosen(KickFlowResult result, KickMode mode) const;

  KickFlowPort &port_;
  KickPage page_ = KickPage::Today;
  KickPage compare_origin_ = KickPage::Insights; // where Compare was opened
  KickPeriod period_ = KickPeriod::Morning;
  KickCounter counter_;
  PregnancyDate today_ = {};
  uint8_t hour_ = 0U;
  uint8_t minute_ = 0U;
  bool save_failed_ = false;
  // The feel page was opened straight from Start (remembered method), so Back
  // goes to Today rather than to a method list the user never saw.
  bool feel_direct_ = false;
  KickSession result_session_ = {};
};
