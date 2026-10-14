#include <cassert>
#include <fstream>
#include <vector>

#include "canvas.h"
#include "ui_language.h"
#include "weight_pages.h"

void preview(Canvas &canvas, const char *path) {
  std::ofstream file(path, std::ios::binary);
  file << "P6\n" << canvas.width() << " " << canvas.height() << "\n255\n";
  for (int y = 0; y < canvas.height(); ++y)
    for (int x = 0; x < canvas.width(); ++x) {
      const uint8_t value = canvas.pixel_at(x, y) == GrayLevel::Black ? 0U : 255U;
      for (unsigned component = 0U; component < 3U; ++component)
        file.write(reinterpret_cast<const char *>(&value), 1U);
    }
}

int main() {
  for (uint16_t value = 300U; value <= 2000U; ++value)
    assert(weight_stored_value(weight_display_value(value, true), true) == value);
  WeightService service;
  WeightRecord record = {};
  record.date = {2026U, 10U, 10U};
  record.weight_tenths_kg = 625U;
  assert(service.upsert(record));
  WeightProfileSettings settings = {165U, 640U, false};
  WeightPageState state = {};
  state.today = record.date;
  state.edit_date = record.date;
  state.edit_id = service.at(0U)->id;
  state.edit_display_weight = 625U;
  state.profile = settings;
  std::vector<uint8_t> pixels(800U * 480U / 4U);
  Canvas canvas(800U, 480U, pixels.data(), pixels.size());
  for (auto language : {UiLanguage::English, UiLanguage::ChineseSimplified}) {
    ui_language_set(language);
    for (auto rotation : {CanvasRotation::Deg0, CanvasRotation::Deg180,
                          CanvasRotation::Deg90Clockwise, CanvasRotation::Deg90CounterClockwise}) {
      canvas.set_rotation(rotation);
      const int width = canvas.width(), height = canvas.height();
      for (auto page : {WeightPage::Dashboard, WeightPage::Editor, WeightPage::History,
                        WeightPage::Profile, WeightPage::ReplaceConfirm, WeightPage::DeleteConfirm}) {
        state.page = page;
        weight_page_render(canvas, service, settings, state);
        assert(weight_page_action_at(page, width, height, width - 80, 40).action == WeightAction::Back);
        for (int y = 0; y < height; ++y)
          for (int x = 0; x < width; ++x)
            assert(canvas.pixel_at(x, y) == GrayLevel::Black || canvas.pixel_at(x, y) == GrayLevel::White);
      }
      assert(weight_page_action_at(WeightPage::Dashboard, width, height, 60, height - 52).action == WeightAction::AddToday);
      assert(weight_page_action_at(WeightPage::Dashboard, width, height, width / 2, height - 52).action == WeightAction::History);
      assert(weight_page_action_at(WeightPage::Dashboard, width, height, width - 60, height - 52).action == WeightAction::Profile);
      assert(weight_page_action_at(WeightPage::History, width, height, 60, 140).action == WeightAction::SelectRecord);
      assert(weight_page_action_at(WeightPage::History, width, height, width - 60, height - 52).action == WeightAction::Next);
      assert(weight_page_action_at(WeightPage::ReplaceConfirm, width, height, width - 60, height - 52).action == WeightAction::Confirm);
    }
  }
  canvas.set_rotation(CanvasRotation::Deg0);
  state.page = WeightPage::Dashboard;
  weight_page_render(canvas, service, settings, state);
  preview(canvas, "/tmp/weight_dashboard_zh.ppm");
  state.page = WeightPage::History;
  weight_page_render(canvas, service, settings, state);
  preview(canvas, "/tmp/weight_history_zh.ppm");
  canvas.set_rotation(CanvasRotation::Deg90Clockwise);
  state.page = WeightPage::Dashboard;
  weight_page_render(canvas, service, settings, state);
  preview(canvas, "/tmp/weight_dashboard_portrait_zh.ppm");
}
