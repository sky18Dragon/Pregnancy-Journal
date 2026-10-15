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

// Geometry contract: every target is on screen, at least 48 px each way, does
// not overlap another target, and the hit test returns the target under the
// point. (The segmented unit/trend toggles are single targets.)
void test_target_contract() {
  const WeightPage pages[] = {WeightPage::Dashboard, WeightPage::Editor, WeightPage::History,
                              WeightPage::Profile, WeightPage::ReplaceConfirm, WeightPage::DeleteConfirm};
  const int sizes[2][2] = {{800, 480}, {480, 800}};
  for (const auto &size : sizes) {
    for (WeightPage page : pages) {
      WeightTarget targets[16];
      const size_t count = weight_page_targets(page, size[0], size[1], targets, 16U);
      assert(count >= 3U);
      for (size_t a = 0U; a < count; ++a) {
        const auto &r = targets[a].rect;
        assert(r.x >= 0 && r.y >= 0 && r.x + r.width <= size[0] && r.y + r.height <= size[1]);
        assert(r.height >= 48 && r.width >= 48);
        const WeightHit hit = weight_page_action_at(page, size[0], size[1], r.x + r.width / 2, r.y + r.height / 2);
        assert(hit.action == targets[a].action && hit.row == targets[a].row);
        for (size_t b = a + 1U; b < count; ++b) {
          const auto &o = targets[b].rect;
          assert(!(r.x < o.x + o.width && o.x < r.x + r.width && r.y < o.y + o.height && o.y < r.y + r.height));
        }
      }
    }
  }
}

// Back graph: every page reaches the dashboard (which leaves for the launcher).
void test_back_graph() {
  const WeightPage pages[] = {WeightPage::Dashboard, WeightPage::Editor, WeightPage::History,
                              WeightPage::Profile, WeightPage::ReplaceConfirm, WeightPage::DeleteConfirm};
  for (bool from_history : {false, true}) {
    for (WeightPage page : pages) {
      WeightPage current = page, next = WeightPage::Dashboard;
      int steps = 0;
      while (weight_page_back_target(current, from_history, next)) {
        assert(next != current);
        current = next;
        assert(++steps <= 4);
      }
      assert(current == WeightPage::Dashboard);
    }
  }
  WeightPage target = WeightPage::Dashboard;
  assert(weight_page_back_target(WeightPage::Editor, true, target) && target == WeightPage::History);
  assert(weight_page_back_target(WeightPage::Editor, false, target) && target == WeightPage::Dashboard);
  assert(weight_page_back_target(WeightPage::DeleteConfirm, true, target) && target == WeightPage::Editor);
}

int main() {
  test_back_graph();
  test_target_contract();
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
