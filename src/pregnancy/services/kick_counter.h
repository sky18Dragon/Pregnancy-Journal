#pragma once

#include "kick.h"

class KickCounter {
public:
  static constexpr int64_t kDebounceUs = 400000LL;
  static constexpr int64_t kMaximumSessionUs = 3600000000LL;
  bool begin(const PregnancyDate &date, uint8_t hour, uint8_t minute,
             KickPeriod period, int64_t now,
             KickMode mode = KickMode::Timed);
  bool add(int64_t now);
  bool undo();
  bool expired(int64_t now) const;
  // CountToTen sessions stop accepting presses once ten are recorded.
  bool target_reached() const;
  uint16_t elapsed_minutes(int64_t now) const;
  uint16_t elapsed_seconds(int64_t now) const;
  KickSession snapshot(int64_t now) const;
  uint16_t count() const { return session_.count; }
  KickMode mode() const { return session_.mode; }
  void cancel() { active_ = false; session_ = {}; }
private:
  KickSession session_ = {};
  int64_t started_ = 0;
  int64_t last_press_ = 0;
  // Fixed when the target is reached so the saved time is the time to ten.
  int64_t reached_at_ = 0;
  bool has_press_ = false;
  bool active_ = false;
};
