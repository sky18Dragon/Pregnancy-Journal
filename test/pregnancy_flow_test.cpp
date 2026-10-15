#include <cassert>
#include <cstring>

#include "pregnancy_flow.h"

namespace {

// A fake RTC: can be unavailable, can reject writes, and records writes.
class FakeClock final : public PregnancyClockPort {
public:
  bool ready = true;
  bool accept_write = true;
  PregnancyClockTime now = {2026U, 10U, 10U, 14U, 42U, 0U};
  int writes = 0;
  int reads = 0;
  bool read(PregnancyClockTime &time) override {
    ++reads;
    if (!ready) return false;
    time = now;
    return true;
  }
  bool write(const PregnancyClockTime &time) override {
    if (!accept_write) return false;
    now = time;
    ++writes;
    return true;
  }
};

void press(PregnancyFlow &flow, const char *digits) {
  for (const char *p = digits; *p; ++p) {
    const PregnancyFlowResult r = flow.handle_action(static_cast<PregnancyAction>(
        static_cast<int>(PregnancyAction::Digit0) + (*p - '0')));
    assert(r.render && !flow.input_error());
  }
}

PregnancyProfile due_profile(uint16_t y, uint8_t m, uint8_t d) {
  PregnancyProfile profile = {};
  assert(pregnancy_profile_from_due_date({y, m, d}, profile));
  return profile;
}

} // namespace

int main() {
  // No RTC: the first page is the clock setup with empty fields.
  {
    FakeClock clock;
    clock.ready = false;
    PregnancyFlow flow(clock);
    flow.begin();
    assert(flow.page() == PregnancyPage::ClockSetup);
    assert(!flow.configured());
    assert(std::strcmp(flow.clock_fields().digits(), "____________") == 0);
  }

  // RTC present: source setup, clock digits prefilled from the RTC.
  FakeClock clock;
  PregnancyFlow flow(clock);
  flow.begin();
  assert(flow.page() == PregnancyPage::SourceSetup);
  assert(std::strcmp(flow.clock_fields().digits(), "202610101442") == 0);

  // Due-date entry: select, an impossible digit is refused, a bad date errors.
  assert(flow.handle_action(PregnancyAction::SelectDueDate).render);
  assert(flow.page() == PregnancyPage::DateSetup && flow.due_date_primary());
  assert(std::strcmp(flow.date_fields().digits(), "________") == 0);
  assert(flow.handle_action(PregnancyAction::Digit3).render && flow.input_error());
  press(flow, "20270230"); // 30 February: every digit is allowed, the date is not
  PregnancyFlowResult result = flow.handle_action(PregnancyAction::Continue);
  assert(result.render && !result.save_profile && flow.input_error());
  // Fix the day and continue: a save is requested and the progress is ready.
  flow.handle_action(PregnancyAction::Field2);
  press(flow, "20");
  assert(!flow.input_error());
  result = flow.handle_action(PregnancyAction::Continue);
  assert(result.render && result.save_profile && !flow.input_error());
  assert(flow.pending_profile().estimated_due_date.month == 2U);
  assert(flow.progress().weeks == 21);
  assert(!flow.configured()); // not configured until storage confirms
  // Storage confirms: configured, dashboard on settle.
  flow.profile_saved();
  assert(flow.configured() && flow.profile().estimated_due_date.day == 20U);
  assert(flow.settle_after_storage(false));
  assert(flow.page() == PregnancyPage::Dashboard);

  // Dashboard navigation.
  assert(flow.handle_action(PregnancyAction::ShowBaby).render &&
         flow.page() == PregnancyPage::Baby);
  assert(flow.handle_action(PregnancyAction::ShowMom).render &&
         flow.page() == PregnancyPage::Mom);
  assert(flow.handle_action(PregnancyAction::ShowOverview).render &&
         flow.page() == PregnancyPage::Dashboard);
  result = flow.handle_action(PregnancyAction::OpenLauncher);
  assert(result.open_launcher && !result.render);
  result = flow.handle_action(PregnancyAction::None);
  assert(!result.render && !result.open_launcher && !result.save_profile);

  // Editing the saved profile starts from its current values; Back returns to
  // the source choice, and Back from there returns to the dashboard.
  flow.handle_action(PregnancyAction::SelectLmp);
  assert(flow.page() == PregnancyPage::DateSetup && !flow.due_date_primary());
  flow.handle_action(PregnancyAction::Back);
  assert(flow.page() == PregnancyPage::SourceSetup);
  flow.handle_action(PregnancyAction::SelectDueDate);
  assert(std::strcmp(flow.date_fields().digits(), "20270220") == 0);
  flow.handle_action(PregnancyAction::Back);
  flow.handle_action(PregnancyAction::Back);
  assert(flow.page() == PregnancyPage::Dashboard);

  // Clock setup: invalid time or a rejected write keeps the page with an error;
  // a good time is written and returns to the dashboard (profile exists).
  flow.begin();
  flow.profile_loaded(true, due_profile(2027U, 2U, 20U));
  assert(flow.configured() && flow.page() == PregnancyPage::SourceSetup);
  assert(flow.settle_after_storage(false) && flow.page() == PregnancyPage::Dashboard);
  flow.handle_action(PregnancyAction::SelectDueDate); // any digit page
  flow.handle_action(PregnancyAction::Back);
  flow.handle_action(PregnancyAction::Back);
  assert(flow.page() == PregnancyPage::Dashboard);

  // Not configured + Back does nothing but redraw.
  {
    FakeClock c2;
    PregnancyFlow f2(c2);
    f2.begin();
    assert(f2.page() == PregnancyPage::SourceSetup);
    const PregnancyFlowResult r = f2.handle_action(PregnancyAction::Back);
    assert(r.render && f2.page() == PregnancyPage::SourceSetup);
    assert(!f2.settle_after_storage(false) && f2.page() == PregnancyPage::SourceSetup);
  }

  // Setup requested (from Settings) wins over an existing profile.
  {
    FakeClock c3;
    PregnancyFlow f3(c3);
    f3.begin();
    f3.profile_loaded(true, due_profile(2027U, 2U, 20U));
    assert(!f3.settle_after_storage(true));
    assert(f3.page() == PregnancyPage::SourceSetup);
  }

  // A clock page never leaves on storage events.
  {
    FakeClock c4;
    c4.ready = false;
    PregnancyFlow f4(c4);
    f4.begin();
    f4.profile_loaded(true, due_profile(2027U, 2U, 20U));
    assert(!f4.settle_after_storage(false));
    assert(f4.page() == PregnancyPage::ClockSetup);
    // Enter a valid time: written, then source/dashboard by configuration.
    press(f4, "202610101530");
    assert(f4.clock_fields().complete());
    c4.ready = true;
    PregnancyFlowResult r = f4.handle_action(PregnancyAction::Continue);
    assert(r.render && !f4.input_error() && c4.writes == 1);
    assert(c4.now.hour == 15U && c4.now.minute == 30U);
    assert(f4.page() == PregnancyPage::Dashboard);
    // A rejected write keeps the page and flags the error.
    f4.handle_action(PregnancyAction::SelectLmp);
  }
  {
    FakeClock c5;
    c5.ready = false;
    PregnancyFlow f5(c5);
    f5.begin();
    press(f5, "202610101530");
    c5.ready = true;
    c5.accept_write = false;
    f5.handle_action(PregnancyAction::Continue);
    assert(f5.input_error() && f5.page() == PregnancyPage::ClockSetup);
    // Impossible clock (month 00 can't be typed, but 31 Nov can).
    f5.handle_action(PregnancyAction::Field1);
    press(f5, "11");
    f5.handle_action(PregnancyAction::Field2);
    press(f5, "31");
    c5.accept_write = true;
    f5.handle_action(PregnancyAction::Continue);
    assert(f5.input_error() && c5.writes == 0 && f5.page() == PregnancyPage::ClockSetup);
  }

  // Midnight rollover recalculates the progress on the next new day only.
  {
    FakeClock c6;
    PregnancyFlow f6(c6);
    f6.begin();
    f6.profile_loaded(true, due_profile(2027U, 2U, 20U));
    assert(f6.settle_after_storage(false));
    const int days_before = f6.progress().days_until_due_date;
    assert(!f6.refresh_for_new_day()); // same day
    c6.now.day = 11U;
    assert(f6.refresh_for_new_day());
    assert(f6.progress().days_until_due_date == days_before - 1);
    assert(!f6.refresh_for_new_day());
    c6.ready = false;
    assert(!f6.refresh_for_new_day());
    assert(f6.showing_week_pages());
  }
  return 0;
}
