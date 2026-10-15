#include "kick_flow.h"

bool KickFlow::read_clock() {
  KickClockTime time;
  if (!port_.read_clock(time))
    return false;
  today_ = time.date;
  hour_ = time.hour;
  minute_ = time.minute;
  return true;
}

bool KickFlow::refresh_clock() {
  const PregnancyDate before = today_;
  return read_clock() &&
         (before.year != today_.year || before.month != today_.month ||
          before.day != today_.day);
}

KickPeriod KickFlow::inferred_period() const {
  if (hour_ < 12U)
    return KickPeriod::Morning;
  if (hour_ < 18U)
    return KickPeriod::Afternoon;
  return KickPeriod::Evening;
}

KickFlowResult KickFlow::save_feel(KickFeel feel) {
  KickFlowResult result;
  result.render = true;
  if (!read_clock()) {
    save_failed_ = true;
    page_ = KickPage::Today;
    return result;
  }
  KickSession session = {};
  session.date = today_;
  session.hour = hour_;
  session.minute = minute_;
  session.period = inferred_period();
  session.mode = KickMode::Awareness;
  session.feel = feel;
  result.save_session = true;
  result.session = session;
  return result;
}

KickFlowResult KickFlow::begin_counter(KickMode mode, int64_t now_us) {
  KickFlowResult result;
  result.render = true;
  if (!read_clock()) {
    save_failed_ = true;
    page_ = KickPage::Today;
    return result;
  }
  period_ = inferred_period();
  counter_.cancel();
  counter_.begin(today_, hour_, minute_, period_, now_us, mode);
  save_failed_ = false;
  page_ = KickPage::Counter;
  return result;
}

KickFlowResult KickFlow::finish(int64_t now_us) {
  KickFlowResult result;
  result.render = true;
  if (counter_.count() == 0U) {
    counter_.cancel();
    page_ = KickPage::Today;
    return result;
  }
  result.save_session = true;
  result.session = counter_.snapshot(now_us);
  return result;
}

void KickFlow::session_saved(const KickSession &session) {
  // Show the saved record next to its own baseline.
  result_session_ = session;
  page_ = KickPage::Result;
  counter_.cancel();
  save_failed_ = false;
}

KickFlowResult KickFlow::handle_action(KickAction action, int64_t now_us) {
  KickFlowResult result;
  switch (action) {
  case KickAction::Back: {
    KickPage target = KickPage::Today;
    if (!kick_page_back_target(page_, compare_origin_, target)) {
      result.open_launcher = true;
      return result;
    }
    if (page_ == KickPage::Counter)
      counter_.cancel();
    page_ = target;
    break;
  }
  case KickAction::Done:
    page_ = KickPage::Today;
    break;
  case KickAction::Start:
    if (port_.at_capacity()) {
      result.capacity_reached = true;
      result.render = true;
      return result;
    }
    save_failed_ = false;
    page_ = KickPage::ModeSelect;
    break;
  case KickAction::StartTimed:
    return begin_counter(KickMode::Timed, now_us);
  case KickAction::StartCountToTen:
    return begin_counter(KickMode::CountToTen, now_us);
  case KickAction::StartAwareness:
    page_ = KickPage::Feel;
    break;
  case KickAction::FeelUsual:
    return save_feel(KickFeel::Usual);
  case KickAction::FeelWeaker:
    return save_feel(KickFeel::Weaker);
  case KickAction::FeelFewer:
    return save_feel(KickFeel::Fewer);
  case KickAction::FeelChanged:
    return save_feel(KickFeel::Changed);
  case KickAction::ShowInsights:
    page_ = KickPage::Insights;
    break;
  case KickAction::ShowCompare:
    read_clock();
    compare_origin_ = page_;
    page_ = KickPage::Compare;
    break;
  case KickAction::Count:
    // The tenth press of a count-to-ten session saves it immediately.
    if (counter_.add(now_us) && counter_.target_reached())
      return finish(now_us);
    break;
  case KickAction::Undo:
    counter_.undo();
    break;
  case KickAction::Finish:
    return finish(now_us);
  case KickAction::None:
    return result;
  }
  result.render = true;
  return result;
}
