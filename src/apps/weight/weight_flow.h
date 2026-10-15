#pragma once

#include <cstddef>
#include <cstdint>

#include "weight_pages.h"
#include "weight_service.h"

// Page and editing flow of the Weight app, free of hardware: which page is
// shown, the value being edited, history paging, the baseline profile, and
// which record or preference write an action asks for. The app task owns the
// display, touch, RTC and the repository; it drives this class and carries out
// the requested writes. The clock and the stored records/settings are reached
// through WeightFlowPort so host tests can fake them.
// 体重应用的页面与编辑流程（页面切换、编辑值、历史翻页、档案、要写入的记录/设置），不接触硬件。

class WeightFlowPort {
public:
  virtual ~WeightFlowPort() = default;
  // False when the RTC cannot be read.
  virtual bool read_today(PregnancyDate &today) = 0;
  virtual const WeightService &service() const = 0;
  virtual WeightProfileSettings settings() const = 0;
};

// What the app has to do after an action.
struct WeightFlowResult {
  bool render = false;          // redraw the (possibly new) page
  bool open_launcher = false;   // hand over to the launcher (no redraw)
  bool invalid_clock = false;   // today's date is unreadable: report and redraw
  bool save_record = false;     // upsert `record`
  WeightRecord record = {};
  // With save_record: write `settings` too once the record is durable (the
  // first record also sets the baseline). With save_preferences: write them now.
  bool preferences_after_record = false;
  bool save_preferences = false;
  WeightProfileSettings settings = {};
  bool erase_record = false;    // delete record `erase_id`
  uint32_t erase_id = 0U;
};

class WeightFlow {
public:
  explicit WeightFlow(WeightFlowPort &port) : port_(port) {}

  WeightFlowResult handle_action(const WeightHit &hit);

  // Clock: `read_today` refreshes today's date; `refresh_today` also reports a
  // date change and only runs on the dashboard.
  bool read_today();
  bool refresh_today();

  // Storage notifications.
  void reset_to_dashboard() { state_.page = WeightPage::Dashboard; }
  // A record or erase became durable: back to the list it came from.
  void record_saved() {
    state_.page = state_.editing_history ? WeightPage::History
                                         : WeightPage::Dashboard;
  }

  const WeightPageState &state() const { return state_; }
  WeightPage page() const { return state_.page; }

private:
  void open_editor(const WeightRecord *record, bool history);
  WeightFlowResult save_record();

  WeightFlowPort &port_;
  WeightPageState state_ = {};
};
