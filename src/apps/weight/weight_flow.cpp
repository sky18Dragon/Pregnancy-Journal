#include "weight_flow.h"

bool WeightFlow::read_today() {
  PregnancyDate today = {};
  if (!port_.read_today(today)) return false;
  state_.today = today;
  return pregnancy_date_valid(state_.today);
}

bool WeightFlow::refresh_today() {
  if (state_.page != WeightPage::Dashboard) return false;
  const PregnancyDate before = state_.today;
  return read_today() &&
         (before.year != state_.today.year || before.month != state_.today.month ||
          before.day != state_.today.day);
}

void WeightFlow::open_editor(const WeightRecord *record, bool history) {
  const WeightService &service = port_.service();
  const WeightProfileSettings settings = port_.settings();
  state_.editing_history = history;
  state_.edit_id = record ? record->id : 0U;
  state_.edit_date = record ? record->date : state_.today;
  const WeightRecord *latest = service.latest();
  // Start from the record, else the latest one, else the pre-pregnancy
  // baseline from the profile (close to the first reading), else 60.0 kg.
  const uint16_t baseline = settings.baseline_tenths_kg;
  state_.edit_display_weight = weight_display_value(
      record ? record->weight_tenths_kg
             : latest ? latest->weight_tenths_kg
                      : baseline != 0U ? baseline : 600U,
      settings.use_pounds);
  state_.page = WeightPage::Editor;
}

WeightFlowResult WeightFlow::save_record() {
  const WeightService &service = port_.service();
  const WeightProfileSettings current = port_.settings();
  WeightFlowResult result;
  result.render = true;
  WeightRecord record = {};
  if (state_.edit_id != 0U) {
    const WeightRecord *existing = service.find(state_.edit_id);
    // Preserve notes while editing the number.
    if (existing != nullptr) record = *existing;
  }
  record.id = state_.edit_id;
  record.date = state_.edit_date;
  record.weight_tenths_kg =
      weight_stored_value(state_.edit_display_weight, current.use_pounds);
  result.settings = current;
  // The first record also fixes the baseline when none is set yet.
  result.preferences_after_record = current.baseline_tenths_kg == 0U;
  if (result.preferences_after_record) {
    const WeightRecord *first =
        service.count() == 0U ? nullptr : service.history_at(service.count() - 1U);
    result.settings.baseline_tenths_kg =
        first ? first->weight_tenths_kg : record.weight_tenths_kg;
  }
  result.save_record = true;
  result.record = record;
  return result;
}

WeightFlowResult WeightFlow::enter_record_today() {
  switch (state_.page) {
  case WeightPage::Editor:
  case WeightPage::Profile:
  case WeightPage::ReplaceConfirm:
  case WeightPage::DeleteConfirm:
    return WeightFlowResult();
  case WeightPage::Dashboard:
  case WeightPage::History:
    break;
  }
  WeightHit hit;
  hit.action = WeightAction::AddToday;
  return handle_action(hit);
}

WeightFlowResult WeightFlow::handle_action(const WeightHit &hit) {
  WeightFlowResult result;
  const WeightService &service = port_.service();
  const bool pounds = port_.settings().use_pounds;
  switch (hit.action) {
  case WeightAction::None:
    return result;
  case WeightAction::Back: {
    WeightPage target = WeightPage::Dashboard;
    if (!weight_page_back_target(state_.page, state_.editing_history, target)) {
      result.open_launcher = true;
      return result;
    }
    state_.page = target;
    break;
  }
  case WeightAction::ToggleUnit: {
    WeightProfileSettings settings = port_.settings();
    settings.use_pounds = !settings.use_pounds;
    state_.editing_history = false;
    result.save_preferences = true;
    result.settings = settings;
    break;
  }
  case WeightAction::ToggleTrend:
    state_.trend_days = state_.trend_days == 7U ? 30U : 7U;
    break;
  case WeightAction::AddToday:
    if (!read_today()) {
      result.invalid_clock = true;
      break;
    }
    open_editor(service.find_date(state_.today), false);
    break;
  case WeightAction::History:
    state_.history_offset = 0U;
    state_.page = WeightPage::History;
    break;
  case WeightAction::Profile:
    state_.profile = port_.settings();
    if (state_.profile.baseline_tenths_kg == 0U)
      state_.profile.baseline_tenths_kg = 600U;
    state_.editing_history = false;
    state_.page = WeightPage::Profile;
    break;
  case WeightAction::SelectRecord: {
    const WeightRecord *record =
        service.history_at(state_.history_offset + hit.row);
    if (record) open_editor(record, true);
    break;
  }
  case WeightAction::Previous:
    if (state_.history_offset >= kWeightHistoryRows)
      state_.history_offset -= kWeightHistoryRows;
    break;
  case WeightAction::Next:
    if (state_.history_offset + kWeightHistoryRows < service.count())
      state_.history_offset += kWeightHistoryRows;
    break;
  case WeightAction::MinusOne:
  case WeightAction::MinusTenth:
  case WeightAction::PlusTenth:
  case WeightAction::PlusOne: {
    const int delta = hit.action == WeightAction::MinusOne     ? -10
                      : hit.action == WeightAction::MinusTenth ? -1
                      : hit.action == WeightAction::PlusTenth  ? 1
                                                               : 10;
    const int value = static_cast<int>(state_.edit_display_weight) + delta;
    if (value >= weight_display_value(300U, pounds) &&
        value <= weight_display_value(2000U, pounds))
      state_.edit_display_weight = static_cast<uint16_t>(value);
    break;
  }
  case WeightAction::HeightDownBig:
    state_.profile.height_cm =
        state_.profile.height_cm >= 105U ? state_.profile.height_cm - 5U : 100U;
    break;
  case WeightAction::HeightUpBig:
    state_.profile.height_cm =
        state_.profile.height_cm <= 215U ? state_.profile.height_cm + 5U : 220U;
    break;
  case WeightAction::BaselineDownBig:
    state_.profile.baseline_tenths_kg =
        state_.profile.baseline_tenths_kg >= 310U
            ? state_.profile.baseline_tenths_kg - 10U
            : 300U;
    break;
  case WeightAction::BaselineUpBig:
    state_.profile.baseline_tenths_kg =
        state_.profile.baseline_tenths_kg <= 1990U
            ? state_.profile.baseline_tenths_kg + 10U
            : 2000U;
    break;
  case WeightAction::HeightDown:
    if (state_.profile.height_cm > 100U) --state_.profile.height_cm;
    break;
  case WeightAction::HeightUp:
    if (state_.profile.height_cm < 220U) ++state_.profile.height_cm;
    break;
  case WeightAction::BaselineDown:
    if (state_.profile.baseline_tenths_kg > 300U)
      --state_.profile.baseline_tenths_kg;
    break;
  case WeightAction::BaselineUp:
    if (state_.profile.baseline_tenths_kg < 2000U)
      ++state_.profile.baseline_tenths_kg;
    break;
  case WeightAction::Save:
    if (state_.page == WeightPage::Profile) {
      result.save_preferences = true;
      result.settings = state_.profile;
    } else if (service.find_date(state_.edit_date) != nullptr) {
      state_.page = WeightPage::ReplaceConfirm;
    } else {
      return save_record();
    }
    break;
  case WeightAction::Delete:
    if (state_.edit_id != 0U) state_.page = WeightPage::DeleteConfirm;
    break;
  case WeightAction::Cancel:
    state_.page = WeightPage::Editor;
    break;
  case WeightAction::Confirm:
    if (state_.page == WeightPage::ReplaceConfirm) {
      return save_record();
    }
    result.erase_record = true;
    result.erase_id = state_.edit_id;
    break;
  }
  result.render = true;
  return result;
}
