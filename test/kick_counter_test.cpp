#include <cassert>
#include "kick_counter.h"

int main() {
  KickCounter counter;
  assert(counter.begin({2026U, 10U, 10U}, 23U, 59U, KickPeriod::Evening, 0));
  assert(counter.add(0));  // Timer zero is a valid first press, not a sentinel.
  assert(!counter.add(399999));
  assert(counter.add(400000));
  assert(counter.undo());
  assert(!counter.add(400001));
  assert(counter.add(800000));
  assert(counter.elapsed_minutes(1) == 1U);
  assert(counter.elapsed_minutes(60000000) == 1U);
  assert(counter.elapsed_minutes(60000001) == 2U);
  assert(!counter.expired(KickCounter::kMaximumSessionUs - 1));
  assert(counter.expired(KickCounter::kMaximumSessionUs));
  assert(!counter.add(KickCounter::kMaximumSessionUs));
  const auto saved = counter.snapshot(KickCounter::kMaximumSessionUs + 1000);
  assert(saved.count == 2U && saved.duration_minutes == 60U);
  assert(saved.date.day == 10U && saved.hour == 23U && saved.minute == 59U);
  counter.cancel();
  assert(counter.count() == 0U && !counter.add(100));
  assert(!counter.begin({2026U, 2U, 30U}, 9U, 0U, KickPeriod::Morning, 0));
  assert(counter.begin({2026U, 10U, 10U}, 9U, 0U, KickPeriod::Morning, 100));
  assert(!counter.add(99));
  for (unsigned count = 0U; count < 500U; ++count)
    assert(counter.add(100LL + count * KickCounter::kDebounceUs));
  assert(!counter.add(100LL + 500LL * KickCounter::kDebounceUs));

  // Timed snapshots carry second-level elapsed time for hourly rates.
  assert(counter.begin({2026U, 10U, 10U}, 9U, 0U, KickPeriod::Morning, 0));
  assert(counter.add(1000000));
  const auto timed = counter.snapshot(1800000000LL);
  assert(timed.mode == KickMode::Timed && timed.elapsed_seconds == 1800U);
  assert(timed.duration_minutes == 30U);

  // Count-to-ten stops at ten and records the time the tenth press arrived.
  assert(counter.begin({2026U, 10U, 10U}, 20U, 0U, KickPeriod::Evening, 0,
                       KickMode::CountToTen));
  for (unsigned press = 0U; press < 10U; ++press) {
    assert(!counter.target_reached());
    assert(counter.add(static_cast<int64_t>(press + 1U) * 61000000LL));
  }
  assert(counter.target_reached() && counter.count() == 10U);
  assert(!counter.add(700000000LL));
  const auto ten = counter.snapshot(900000000LL);
  assert(ten.mode == KickMode::CountToTen && ten.count == 10U);
  assert(ten.elapsed_seconds == 610U && ten.duration_minutes == 11U);
  assert(!counter.begin({2026U, 10U, 10U}, 9U, 0U, KickPeriod::Morning, 0,
                        KickMode::Awareness));
}
