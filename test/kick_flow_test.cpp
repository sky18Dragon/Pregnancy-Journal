#include <cassert>

#include "kick_flow.h"

namespace {

class FakePort final : public KickFlowPort {
public:
  bool clock_ok = true;
  bool full = false;
  KickClockTime now = {{2026U, 10U, 10U}, 14U, 30U};
  bool read_clock(KickClockTime &time) override {
    if (!clock_ok) return false;
    time = now;
    return true;
  }
  bool at_capacity() override { return full; }
};

constexpr int64_t kSecond = 1000000LL;

} // namespace

int main() {
  FakePort port;
  KickFlow flow(port);
  assert(flow.page() == KickPage::Today);

  // Today -> Start -> record type; Back climbs one level at a time.
  KickFlowResult r = flow.handle_action(KickAction::Start, 0);
  assert(r.render && !r.capacity_reached && flow.page() == KickPage::ModeSelect);
  r = flow.handle_action(KickAction::StartAwareness, 0);
  assert(r.render && flow.page() == KickPage::Feel);
  r = flow.handle_action(KickAction::Back, 0);
  assert(r.render && flow.page() == KickPage::ModeSelect);
  r = flow.handle_action(KickAction::Back, 0);
  assert(r.render && flow.page() == KickPage::Today);
  // Back from the root leaves for the launcher without redrawing.
  r = flow.handle_action(KickAction::Back, 0);
  assert(r.open_launcher && !r.render && flow.page() == KickPage::Today);
  r = flow.handle_action(KickAction::None, 0);
  assert(!r.render && !r.open_launcher && !r.save_session);

  // Record limit: refuse to start and say why.
  port.full = true;
  r = flow.handle_action(KickAction::Start, 0);
  assert(r.capacity_reached && r.render && flow.page() == KickPage::Today);
  port.full = false;

  // A feel check-in asks to save an awareness record stamped with the clock;
  // an unreadable RTC fails the save and returns to Today.
  flow.handle_action(KickAction::Start, 0);
  flow.handle_action(KickAction::StartAwareness, 0);
  r = flow.handle_action(KickAction::FeelWeaker, 0);
  assert(r.save_session && r.session.mode == KickMode::Awareness &&
         r.session.feel == KickFeel::Weaker && r.session.hour == 14U &&
         r.session.period == KickPeriod::Afternoon && r.session.date.day == 10U);
  port.clock_ok = false;
  r = flow.handle_action(KickAction::FeelUsual, 0);
  assert(!r.save_session && r.render && flow.save_failed() &&
         flow.page() == KickPage::Today);
  port.clock_ok = true;

  // Timed counter: begins with the inferred period; taps count (debounced),
  // Undo removes one, Back cancels without saving.
  flow.handle_action(KickAction::Start, 0);
  assert(!flow.save_failed());
  r = flow.handle_action(KickAction::StartTimed, 10 * kSecond);
  assert(r.render && flow.page() == KickPage::Counter);
  assert(flow.period() == KickPeriod::Afternoon && flow.counter().count() == 0U);
  flow.handle_action(KickAction::Count, 20 * kSecond);
  flow.handle_action(KickAction::Count, 20 * kSecond + 100000); // inside debounce
  flow.handle_action(KickAction::Count, 30 * kSecond);
  assert(flow.counter().count() == 2U);
  flow.handle_action(KickAction::Undo, 31 * kSecond);
  assert(flow.counter().count() == 1U);
  r = flow.handle_action(KickAction::Back, 32 * kSecond);
  assert(r.render && flow.page() == KickPage::Today && flow.counter().count() == 0U);

  // Finish with nothing counted drops the session; with taps it asks to save.
  flow.handle_action(KickAction::Start, 0);
  flow.handle_action(KickAction::StartTimed, 0);
  r = flow.handle_action(KickAction::Finish, 5 * kSecond);
  assert(!r.save_session && flow.page() == KickPage::Today);
  flow.handle_action(KickAction::Start, 0);
  flow.handle_action(KickAction::StartTimed, 0);
  flow.handle_action(KickAction::Count, 10 * kSecond);
  r = flow.handle_action(KickAction::Finish, 70 * kSecond);
  assert(r.save_session && r.session.count == 1U && r.session.mode == KickMode::Timed);
  // The record is durable: show the result page and clear the draft.
  flow.session_saved(r.session);
  assert(flow.page() == KickPage::Result && flow.counter().count() == 0U &&
         flow.result_session().count == 1U && !flow.save_failed());

  // Count to ten saves by itself on the tenth tap.
  flow.handle_action(KickAction::Done, 0);
  assert(flow.page() == KickPage::Today);
  flow.handle_action(KickAction::Start, 0);
  flow.handle_action(KickAction::StartCountToTen, 100 * kSecond);
  for (int i = 0; i < 9; ++i) {
    r = flow.handle_action(KickAction::Count, (110 + 10 * i) * kSecond);
    assert(!r.save_session);
  }
  r = flow.handle_action(KickAction::Count, 200 * kSecond);
  assert(r.save_session && r.session.mode == KickMode::CountToTen && r.session.count == 10U);

  // Top-key presses are counted at the time they were made. Presses that waited
  // out a screen refresh arrive together but keep their own times, so the
  // debounce only merges presses that really were closer than 400 ms.
  flow.abandon_session();
  flow.handle_action(KickAction::Start, 0);
  flow.handle_action(KickAction::StartTimed, 1000 * kSecond);
  flow.handle_action(KickAction::Count, 1010 * kSecond);
  flow.handle_action(KickAction::Count, 1010 * kSecond + 900000);
  flow.handle_action(KickAction::Count, 1011 * kSecond);          // 100 ms later
  flow.handle_action(KickAction::Count, 1011 * kSecond + 400000); // 500 ms later
  assert(flow.counter().count() == 3U);
  // A press made before the session began (it was queued across a page change)
  // is not a movement of this session.
  flow.handle_action(KickAction::Count, 999 * kSecond);
  assert(flow.counter().count() == 3U);

  // The time to ten ends at the tenth press, not when it is processed.
  flow.abandon_session();
  flow.handle_action(KickAction::Start, 0);
  flow.handle_action(KickAction::StartCountToTen, 2000 * kSecond);
  for (int i = 1; i <= 9; ++i)
    flow.handle_action(KickAction::Count, (2000 + 5 * i) * kSecond);
  r = flow.handle_action(KickAction::Count, 2050 * kSecond);
  assert(r.save_session && r.session.count == 10U &&
         r.session.elapsed_seconds == 50U);
  flow.abandon_session();

  // The counter expires after an hour; only on the counter page.
  flow.abandon_session();
  assert(flow.page() == KickPage::Today && flow.counter().count() == 0U);
  flow.handle_action(KickAction::Start, 0);
  flow.handle_action(KickAction::StartTimed, 0);
  flow.handle_action(KickAction::Count, 10 * kSecond);
  assert(!flow.counter_expired(3000 * kSecond));
  assert(flow.counter_expired(3700 * kSecond));
  flow.go_today();
  assert(!flow.counter_expired(3700 * kSecond));

  // Compare returns to the page it was opened from.
  flow.handle_action(KickAction::ShowInsights, 0);
  assert(flow.page() == KickPage::Insights);
  flow.handle_action(KickAction::ShowCompare, 0);
  assert(flow.page() == KickPage::Compare);
  flow.handle_action(KickAction::Back, 0);
  assert(flow.page() == KickPage::Insights);
  flow.session_saved(r.session);
  flow.handle_action(KickAction::ShowCompare, 0);
  flow.handle_action(KickAction::Back, 0);
  assert(flow.page() == KickPage::Result);

  // Clock refresh reports a date change once.
  flow.go_today();
  assert(!flow.refresh_clock());
  port.now.date.day = 11U;
  assert(flow.refresh_clock() && flow.today().day == 11U);
  assert(!flow.refresh_clock());
  port.clock_ok = false;
  assert(!flow.refresh_clock());
  return 0;
}
