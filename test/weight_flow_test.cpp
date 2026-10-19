#include <cassert>
#include <initializer_list>

#include "weight_flow.h"

namespace {

class FakePort final : public WeightFlowPort {
public:
  bool clock_ok = true;
  PregnancyDate today = {2026U, 10U, 10U};
  WeightService records;
  WeightProfileSettings prefs = {165U, 0U, false};
  bool read_today(PregnancyDate &out) override {
    if (!clock_ok) return false;
    out = today;
    return true;
  }
  const WeightService &service() const override { return records; }
  WeightProfileSettings settings() const override { return prefs; }
};

WeightHit hit(WeightAction action, size_t row = 0U) { return {action, row}; }

void add_record(FakePort &port, uint8_t day, uint16_t tenths) {
  WeightRecord r = {};
  r.date = {2026U, 10U, day};
  r.weight_tenths_kg = tenths;
  assert(port.records.upsert(r));
}

} // namespace

int main() {
  FakePort port;
  WeightFlow flow(port);
  assert(flow.page() == WeightPage::Dashboard && flow.read_today());
  assert(flow.state().today.day == 10U);

  // Back from the root leaves for the launcher; None does nothing.
  WeightFlowResult r = flow.handle_action(hit(WeightAction::Back));
  assert(r.open_launcher && !r.render);
  r = flow.handle_action(hit(WeightAction::None));
  assert(!r.render && !r.open_launcher);

  // Trend toggle and history paging.
  r = flow.handle_action(hit(WeightAction::ToggleTrend));
  assert(r.render && flow.state().trend_days == 30U);
  flow.handle_action(hit(WeightAction::ToggleTrend));
  assert(flow.state().trend_days == 7U);

  // First record: the editor starts from 60.0 kg without a baseline, and the
  // save also sets the baseline from this record.
  r = flow.handle_action(hit(WeightAction::AddToday));
  assert(r.render && flow.page() == WeightPage::Editor);
  assert(flow.state().edit_display_weight == 600U && flow.state().edit_id == 0U);
  flow.handle_action(hit(WeightAction::PlusOne));
  flow.handle_action(hit(WeightAction::PlusTenth));
  assert(flow.state().edit_display_weight == 611U);
  flow.handle_action(hit(WeightAction::MinusTenth));
  assert(flow.state().edit_display_weight == 610U);
  r = flow.handle_action(hit(WeightAction::Save));
  assert(r.save_record && r.record.weight_tenths_kg == 610U &&
         r.record.date.day == 10U && r.preferences_after_record &&
         r.settings.baseline_tenths_kg == 610U && !r.save_preferences);
  // Storage confirms: back to the dashboard.
  flow.record_saved();
  assert(flow.page() == WeightPage::Dashboard);
  // The app carries out the write: the record and then the baseline setting.
  port.records.upsert(r.record);
  port.prefs = r.settings;

  // With a baseline in the profile and no records, the editor starts there.
  {
    FakePort p2;
    p2.prefs.baseline_tenths_kg = 640U;
    WeightFlow f2(p2);
    f2.read_today();
    f2.handle_action(hit(WeightAction::AddToday));
    assert(f2.state().edit_display_weight == 640U);
  }

  // An unset baseline shows 60.0 kg on the profile page.
  {
    FakePort p3;
    WeightFlow f3(p3);
    f3.handle_action(hit(WeightAction::Profile));
    assert(f3.state().profile.baseline_tenths_kg == 600U);
  }

  // Today already has a record: the editor shows it and Save asks to confirm
  // the replacement before writing; Back from the confirmation returns to it.
  r = flow.handle_action(hit(WeightAction::AddToday));
  assert(flow.page() == WeightPage::Editor && flow.state().edit_id != 0U &&
         flow.state().edit_display_weight == 610U);
  r = flow.handle_action(hit(WeightAction::Save));
  assert(!r.save_record && flow.page() == WeightPage::ReplaceConfirm);
  flow.handle_action(hit(WeightAction::Cancel));
  assert(flow.page() == WeightPage::Editor);
  flow.handle_action(hit(WeightAction::Save));
  r = flow.handle_action(hit(WeightAction::Confirm));
  assert(r.save_record && r.record.id == flow.state().edit_id &&
         !r.preferences_after_record);

  // Delete needs a confirmation and then asks to erase that record.
  flow.record_saved();
  flow.handle_action(hit(WeightAction::AddToday));
  r = flow.handle_action(hit(WeightAction::Delete));
  assert(flow.page() == WeightPage::DeleteConfirm && !r.erase_record);
  const uint32_t id = flow.state().edit_id;
  r = flow.handle_action(hit(WeightAction::Confirm));
  assert(r.erase_record && r.erase_id == id);
  flow.record_saved();

  // The editor range is clamped (30.0 - 200.0 kg).
  flow.handle_action(hit(WeightAction::AddToday));
  for (int i = 0; i < 30; ++i) flow.handle_action(hit(WeightAction::PlusOne));
  assert(flow.state().edit_display_weight <= 2000U);
  for (int i = 0; i < 400; ++i) flow.handle_action(hit(WeightAction::MinusOne));
  assert(flow.state().edit_display_weight >= 300U);
  flow.handle_action(hit(WeightAction::Back));
  assert(flow.page() == WeightPage::Dashboard);

  // History: five rows per page, rows open the editor and Back returns there.
  for (uint8_t day = 1U; day <= 9U; ++day) add_record(port, day, 600U + day);
  flow.handle_action(hit(WeightAction::History));
  assert(flow.page() == WeightPage::History && flow.state().history_offset == 0U);
  flow.handle_action(hit(WeightAction::Next));
  assert(flow.state().history_offset == kWeightHistoryRows);
  flow.handle_action(hit(WeightAction::Next));
  assert(flow.state().history_offset == kWeightHistoryRows); // no further page
  flow.handle_action(hit(WeightAction::Previous));
  assert(flow.state().history_offset == 0U);
  flow.handle_action(hit(WeightAction::SelectRecord, 1U));
  assert(flow.page() == WeightPage::Editor && flow.state().editing_history);
  flow.handle_action(hit(WeightAction::Back));
  assert(flow.page() == WeightPage::History);
  flow.handle_action(hit(WeightAction::SelectRecord, 99U)); // no such row
  assert(flow.page() == WeightPage::History);
  flow.handle_action(hit(WeightAction::Back));
  assert(flow.page() == WeightPage::Dashboard);

  // Unit toggle asks to save the flipped preference and leaves history mode.
  r = flow.handle_action(hit(WeightAction::ToggleUnit));
  assert(r.save_preferences && r.settings.use_pounds && r.render);

  // Profile: starts from the stored values, steps are clamped, Save asks to
  // write the edited profile.
  r = flow.handle_action(hit(WeightAction::Profile));
  assert(flow.page() == WeightPage::Profile && flow.state().profile.height_cm == 165U &&
         flow.state().profile.baseline_tenths_kg == 610U);
  flow.handle_action(hit(WeightAction::HeightUpBig));
  flow.handle_action(hit(WeightAction::HeightDown));
  flow.handle_action(hit(WeightAction::BaselineUpBig));
  flow.handle_action(hit(WeightAction::BaselineDown));
  assert(flow.state().profile.height_cm == 169U &&
         flow.state().profile.baseline_tenths_kg == 619U);
  for (int i = 0; i < 40; ++i) flow.handle_action(hit(WeightAction::HeightUpBig));
  assert(flow.state().profile.height_cm == 220U);
  for (int i = 0; i < 40; ++i) flow.handle_action(hit(WeightAction::HeightDownBig));
  assert(flow.state().profile.height_cm == 100U);
  r = flow.handle_action(hit(WeightAction::Save));
  assert(r.save_preferences && r.settings.height_cm == 100U && !r.save_record);

  // An unreadable clock is reported instead of opening the editor.
  flow.reset_to_dashboard();
  port.clock_ok = false;
  r = flow.handle_action(hit(WeightAction::AddToday));
  assert(r.invalid_clock && r.render && flow.page() == WeightPage::Dashboard);
  port.clock_ok = true;

  // The shortcut opens today's entry like "add today"; from the history too.
  {
    FakePort p4;
    WeightFlow f4(p4);
    f4.read_today();
    WeightFlowResult q = f4.enter_record_today();
    assert(q.render && f4.page() == WeightPage::Editor && !f4.state().editing_history &&
           f4.state().edit_id == 0U);
    // A page holding unsaved input is never replaced and asks for no redraw.
    f4.handle_action(hit(WeightAction::PlusOne));
    const uint16_t edited = f4.state().edit_display_weight;
    q = f4.enter_record_today();
    assert(!q.render && !q.save_record && f4.page() == WeightPage::Editor &&
           f4.state().edit_display_weight == edited);
    f4.handle_action(hit(WeightAction::Back));
    assert(f4.page() == WeightPage::Dashboard);
    f4.handle_action(hit(WeightAction::History));
    q = f4.enter_record_today();
    assert(q.render && f4.page() == WeightPage::Editor && !f4.state().editing_history);

    // Today already has a record: the editor opens on it.
    FakePort p5;
    add_record(p5, 10U, 655U);
    WeightFlow f5(p5);
    f5.read_today();
    q = f5.enter_record_today();
    assert(q.render && f5.page() == WeightPage::Editor && f5.state().edit_id != 0U &&
           f5.state().edit_display_weight == 655U);
    for (WeightAction action : {WeightAction::Profile}) {
      f5.reset_to_dashboard();
      f5.handle_action(hit(action));
      assert(f5.page() == WeightPage::Profile);
      q = f5.enter_record_today();
      assert(!q.render && f5.page() == WeightPage::Profile);
    }
    // An unreadable clock is reported, not hidden.
    FakePort p6;
    WeightFlow f6(p6);
    f6.read_today();
    p6.clock_ok = false;
    q = f6.enter_record_today();
    assert(q.invalid_clock && f6.page() == WeightPage::Dashboard);
  }

  // Date change: only noticed on the dashboard, and only once.
  assert(!flow.refresh_today());
  port.today.day = 11U;
  assert(flow.refresh_today() && flow.state().today.day == 11U);
  assert(!flow.refresh_today());
  flow.handle_action(hit(WeightAction::History));
  port.today.day = 12U;
  assert(!flow.refresh_today());
  return 0;
}
