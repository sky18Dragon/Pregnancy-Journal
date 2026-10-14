#include "kick_counter.h"

bool KickCounter::begin(const PregnancyDate &date, uint8_t hour, uint8_t minute,
                        KickPeriod period, int64_t now, KickMode mode) {
  if (!pregnancy_date_valid(date) || hour > 23U || minute > 59U ||
      period > KickPeriod::Evening || now < 0 ||
      (mode != KickMode::Timed && mode != KickMode::CountToTen)) return false;
  session_ = {};
  session_.date = date; session_.hour = hour; session_.minute = minute; session_.period = period;
  session_.mode = mode;
  started_ = now; reached_at_ = 0; has_press_ = false; active_ = true;
  return true;
}
bool KickCounter::add(int64_t now) {
  if (!active_ || now < started_ || expired(now) || session_.count >= 500U ||
      target_reached() ||
      (has_press_ && (now < last_press_ || now - last_press_ < kDebounceUs))) return false;
  ++session_.count; last_press_ = now; has_press_ = true;
  if (target_reached()) reached_at_ = now;
  return true;
}
bool KickCounter::undo() {
  if (!active_ || session_.count == 0U) return false;
  --session_.count;
  return true;
}
bool KickCounter::expired(int64_t now) const {
  return active_ && now >= started_ && now - started_ >= kMaximumSessionUs;
}
bool KickCounter::target_reached() const {
  return active_ && session_.mode == KickMode::CountToTen &&
         session_.count >= kKickCountToTenTarget;
}
uint16_t KickCounter::elapsed_minutes(int64_t now) const {
  if (!active_ || now <= started_) return 0U;
  const auto elapsed = now - started_;
  if (elapsed >= kMaximumSessionUs) return 60U;
  return static_cast<uint16_t>((elapsed + 59999999LL) / 60000000LL);
}
uint16_t KickCounter::elapsed_seconds(int64_t now) const {
  if (!active_) return 0U;
  const int64_t end = target_reached() ? reached_at_ : now;
  if (end <= started_) return 0U;
  const auto elapsed = end - started_;
  if (elapsed >= kMaximumSessionUs)
    return static_cast<uint16_t>(kMaximumSessionUs / 1000000LL);
  return static_cast<uint16_t>(elapsed / 1000000LL);
}
KickSession KickCounter::snapshot(int64_t now) const {
  auto result = session_;
  result.elapsed_seconds = elapsed_seconds(now);
  result.duration_minutes = target_reached()
      ? static_cast<uint16_t>((result.elapsed_seconds + 59U) / 60U)
      : elapsed_minutes(now);
  return result;
}
