#pragma once

#include "prototype_ui.h"
#include "weight_service.h"

enum class WeightPage : uint8_t { Dashboard, Editor, History, Profile, ReplaceConfirm, DeleteConfirm };
enum class WeightAction : uint8_t {
  None, Back, ToggleUnit, ToggleTrend, AddToday, History, Profile, SelectRecord,
  Previous, Next, MinusOne, MinusTenth, PlusTenth, PlusOne, Save, Delete,
  HeightDown, HeightUp, BaselineDown, BaselineUp, Confirm, Cancel
};
constexpr size_t kWeightHistoryRows = 5U;
struct WeightPageState {
  WeightPage page = WeightPage::Dashboard;
  PregnancyDate today = {};
  PregnancyDate edit_date = {};
  uint16_t edit_display_weight = 600U;
  uint32_t edit_id = 0U;
  uint8_t trend_days = 7U;
  size_t history_offset = 0U;
  bool editing_history = false;
  WeightProfileSettings profile = {};
};
struct WeightHit { WeightAction action = WeightAction::None; size_t row = 0U; };
WeightHit weight_page_action_at(WeightPage page, int width, int height, int x, int y);
void weight_page_render(Canvas &canvas, const WeightService &service,
                        const WeightProfileSettings &settings, const WeightPageState &state);
uint16_t weight_display_value(uint16_t tenths_kg, bool pounds);
uint16_t weight_stored_value(uint16_t displayed, bool pounds);
