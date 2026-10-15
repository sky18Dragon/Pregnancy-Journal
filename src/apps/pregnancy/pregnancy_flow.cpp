#include "pregnancy_flow.h"

#include <cstdio>

namespace {

uint16_t digits_u16(const char *digits, size_t offset, size_t count) {
  uint16_t value = 0U;
  for (size_t index = 0U; index < count; ++index) {
    value = static_cast<uint16_t>(
        value * 10U + static_cast<uint16_t>(digits[offset + index] - '0'));
  }
  return value;
}

} // namespace

void PregnancyFlow::fill_clock_digits(const PregnancyClockTime &time) {
  char digits[13] = {};
  std::snprintf(digits, sizeof(digits), "%04u%02u%02u%02u%02u",
                static_cast<unsigned>(time.year % 10000U),
                static_cast<unsigned>(time.month % 100U),
                static_cast<unsigned>(time.day % 100U),
                static_cast<unsigned>(time.hour % 100U),
                static_cast<unsigned>(time.minute % 100U));
  clock_fields_.set_digits(digits);
}

void PregnancyFlow::fill_date_digits(const PregnancyDate &date) {
  char digits[9] = {};
  std::snprintf(digits, sizeof(digits), "%04u%02u%02u",
                static_cast<unsigned>(date.year % 10000U),
                static_cast<unsigned>(date.month % 100U),
                static_cast<unsigned>(date.day % 100U));
  date_fields_.set_digits(digits);
}

bool PregnancyFlow::parse_clock(PregnancyClockTime &time) const {
  if (!clock_fields_.complete())
    return false;
  const char *digits = clock_fields_.digits();
  time.year = digits_u16(digits, 0U, 4U);
  time.month = static_cast<uint8_t>(digits_u16(digits, 4U, 2U));
  time.day = static_cast<uint8_t>(digits_u16(digits, 6U, 2U));
  time.hour = static_cast<uint8_t>(digits_u16(digits, 8U, 2U));
  time.minute = static_cast<uint8_t>(digits_u16(digits, 10U, 2U));
  time.second = 0U;
  return pregnancy_date_valid({time.year, time.month, time.day}) &&
         time.hour <= 23U && time.minute <= 59U;
}

bool PregnancyFlow::parse_profile_date(PregnancyDate &date) const {
  if (!date_fields_.complete())
    return false;
  const char *digits = date_fields_.digits();
  date.year = digits_u16(digits, 0U, 4U);
  date.month = static_cast<uint8_t>(digits_u16(digits, 4U, 2U));
  date.day = static_cast<uint8_t>(digits_u16(digits, 6U, 2U));
  return pregnancy_date_valid(date);
}

bool PregnancyFlow::update_progress_from_clock() {
  PregnancyClockTime time;
  if (!clock_.read(time))
    return false;
  today_ = {time.year, time.month, time.day};
  if (!pregnancy_date_to_day_index(today_, last_day_index_))
    return false;
  fill_clock_digits(time);
  return configured_ &&
         pregnancy_progress_calculate(today_, profile_, progress_);
}

void PregnancyFlow::open_clock_setup() {
  PregnancyClockTime time;
  if (clock_.read(time))
    fill_clock_digits(time);
  else
    clock_fields_.begin(DigitFieldKind::DateTime);
  page_ = PregnancyPage::ClockSetup;
  input_error_ = false;
}

void PregnancyFlow::open_source_setup() {
  page_ = PregnancyPage::SourceSetup;
  input_error_ = false;
}

void PregnancyFlow::open_date_setup(bool due_date_primary) {
  due_date_primary_ = due_date_primary;
  if (configured_) {
    fill_date_digits(due_date_primary ? profile_.estimated_due_date
                                      : profile_.last_menstrual_period);
  } else {
    date_fields_.begin(DigitFieldKind::Date);
  }
  page_ = PregnancyPage::DateSetup;
  input_error_ = false;
}

void PregnancyFlow::begin() {
  PregnancyClockTime time;
  if (!clock_.read(time)) {
    open_clock_setup();
  } else {
    fill_clock_digits(time);
    today_ = {time.year, time.month, time.day};
    pregnancy_date_to_day_index(today_, last_day_index_);
    open_source_setup();
  }
  configured_ = false;
}

void PregnancyFlow::apply_clock() {
  PregnancyClockTime time;
  if (!parse_clock(time) || !clock_.write(time)) {
    input_error_ = true;
    return;
  }
  today_ = {time.year, time.month, time.day};
  pregnancy_date_to_day_index(today_, last_day_index_);
  if (configured_ &&
      pregnancy_progress_calculate(today_, profile_, progress_)) {
    page_ = PregnancyPage::Dashboard;
  } else {
    open_source_setup();
  }
}

PregnancyFlowResult PregnancyFlow::apply_profile_date() {
  PregnancyFlowResult result;
  result.render = true;
  PregnancyDate date = {};
  PregnancyProfile profile = {};
  // The progress is computed while validating, as the dashboard needs it as
  // soon as the profile is saved.
  if (!parse_profile_date(date) ||
      !(due_date_primary_ ? pregnancy_profile_from_due_date(date, profile)
                          : pregnancy_profile_from_lmp(date, profile)) ||
      !pregnancy_progress_calculate(today_, profile, progress_)) {
    input_error_ = true;
    return result;
  }
  pending_profile_ = profile;
  result.save_profile = true;
  return result;
}

PregnancyFlowResult PregnancyFlow::handle_action(PregnancyAction action) {
  PregnancyFlowResult result;
  DigitFields &fields =
      page_ == PregnancyPage::ClockSetup ? clock_fields_ : date_fields_;
  char digit = '\0';
  size_t field = 0U;
  if (pregnancy_action_digit(action, digit)) {
    // An impossible digit (month 13, hour 25...) is refused on the spot.
    input_error_ = !fields.input_digit(digit);
    result.render = true;
    return result;
  }
  if (pregnancy_action_field(action, field)) {
    input_error_ = false;
    fields.select_field(field);
    result.render = true;
    return result;
  }
  switch (action) {
  case PregnancyAction::Delete:
    input_error_ = false;
    fields.erase();
    result.render = true;
    break;
  case PregnancyAction::Continue:
    if (page_ == PregnancyPage::ClockSetup) {
      apply_clock();
      result.render = true;
    } else {
      result = apply_profile_date();
    }
    break;
  case PregnancyAction::SelectDueDate:
    open_date_setup(true);
    result.render = true;
    break;
  case PregnancyAction::SelectLmp:
    open_date_setup(false);
    result.render = true;
    break;
  case PregnancyAction::ShowOverview:
    page_ = PregnancyPage::Dashboard;
    result.render = true;
    break;
  case PregnancyAction::ShowBaby:
    page_ = PregnancyPage::Baby;
    result.render = true;
    break;
  case PregnancyAction::ShowMom:
    page_ = PregnancyPage::Mom;
    result.render = true;
    break;
  case PregnancyAction::Back:
    if (page_ == PregnancyPage::DateSetup)
      open_source_setup();
    else if (configured_) {
      update_progress_from_clock();
      page_ = PregnancyPage::Dashboard;
    }
    result.render = true;
    break;
  case PregnancyAction::OpenLauncher:
    result.open_launcher = true;
    break;
  default:
    break;
  }
  return result;
}

void PregnancyFlow::profile_loaded(bool found, const PregnancyProfile &profile) {
  configured_ = found;
  if (configured_)
    profile_ = profile;
}

void PregnancyFlow::profile_saved() {
  profile_ = pending_profile_;
  configured_ = true;
  input_error_ = false;
}

bool PregnancyFlow::settle_after_storage(bool setup_requested) {
  if (page_ == PregnancyPage::ClockSetup)
    return false;
  if (setup_requested) {
    open_source_setup();
  } else if (configured_ &&
             pregnancy_progress_calculate(today_, profile_, progress_)) {
    page_ = PregnancyPage::Dashboard;
    return true;
  } else {
    open_source_setup();
  }
  return false;
}

bool PregnancyFlow::refresh_for_new_day() {
  PregnancyClockTime time;
  if (!clock_.read(time))
    return false;
  const PregnancyDate today = {time.year, time.month, time.day};
  int32_t day_index = 0;
  if (!pregnancy_date_to_day_index(today, day_index) ||
      day_index == last_day_index_)
    return false;
  today_ = today;
  last_day_index_ = day_index;
  return pregnancy_progress_calculate(today, profile_, progress_);
}
