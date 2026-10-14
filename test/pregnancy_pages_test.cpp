#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

#include "canvas.h"
#include "font.h"
#include "pregnancy_pages.h"
#include "ui_tokens.h"
#include "pregnancy_week_visual.h"
#include "ui_language.h"

namespace {

constexpr uint16_t kWidth = 800U;
constexpr uint16_t kHeight = 480U;
constexpr size_t kStride = kWidth / 4U;

uint8_t pixel_level(const std::vector<uint8_t> &buffer, int x, int y) {
  const size_t index =
      static_cast<size_t>(y) * kStride + static_cast<size_t>(x) / 4U;
  const uint8_t shift = static_cast<uint8_t>((3 - (x & 0x03)) * 2);
  return static_cast<uint8_t>((buffer[index] >> shift) & 0x03U);
}

size_t count_level(const std::vector<uint8_t> &buffer, GrayLevel level) {
  size_t count = 0U;
  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      if (pixel_level(buffer, x, y) == static_cast<uint8_t>(level)) {
        ++count;
      }
    }
  }
  return count;
}

void write_preview(const std::vector<uint8_t> &buffer, const char *path) {
  std::ofstream output(path, std::ios::binary);
  output << "P6\n" << kWidth << " " << kHeight << "\n255\n";
  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      const uint8_t value =
          static_cast<uint8_t>(pixel_level(buffer, x, y) * 85U);
      output.write(reinterpret_cast<const char *>(&value), 1);
      output.write(reinterpret_cast<const char *>(&value), 1);
      output.write(reinterpret_cast<const char *>(&value), 1);
    }
  }
}

void write_canvas_preview(Canvas &canvas, const char *path) {
  std::ofstream output(path, std::ios::binary);
  output << "P6\n" << canvas.width() << " " << canvas.height() << "\n255\n";
  for (int y = 0; y < canvas.height(); ++y) {
    for (int x = 0; x < canvas.width(); ++x) {
      const uint8_t value = static_cast<uint8_t>(canvas.pixel_at(x, y)) * 85U;
      output.write(reinterpret_cast<const char *>(&value), 1);
      output.write(reinterpret_cast<const char *>(&value), 1);
      output.write(reinterpret_cast<const char *>(&value), 1);
    }
  }
}

size_t count_black_rect(Canvas &canvas, int x, int y, int width, int height) {
  size_t count = 0U;
  for (int row = y; row < y + height; ++row) {
    for (int column = x; column < x + width; ++column) {
      if (canvas.pixel_at(column, row) == GrayLevel::Black)
        ++count;
    }
  }
  return count;
}

} // namespace

// The geometry contract: every target of every page is on screen, large enough
// to tap, and does not overlap another target; the hit test returns exactly the
// target under the point.
void test_target_contract() {
  const PregnancyPage pages[] = {
      PregnancyPage::ClockSetup, PregnancyPage::SourceSetup,
      PregnancyPage::DateSetup,  PregnancyPage::Dashboard,
      PregnancyPage::Baby,       PregnancyPage::Mom};
  const int sizes[2][2] = {{800, 480}, {480, 800}};
  for (const auto &size : sizes) {
    for (PregnancyPage page : pages) {
      for (bool can_cancel : {false, true}) {
        PregnancyTarget targets[16];
        const size_t count = pregnancy_page_targets(
            page, can_cancel, size[0], size[1], targets, 16U);
        assert(count > 0U);
        for (size_t a = 0U; a < count; ++a) {
          const auto &r = targets[a].rect;
          assert(r.x >= 0 && r.y >= 0 && r.x + r.width <= size[0] &&
                 r.y + r.height <= size[1]);
          assert(r.height >= ui_tokens::kMinimumTouchHeight);
          assert(r.width >= ui_tokens::kMinimumTouchHeight);
          assert(pregnancy_page_action_at(page, can_cancel, size[0], size[1],
                                          r.x + r.width / 2,
                                          r.y + r.height / 2) ==
                 targets[a].action);
          for (size_t b = a + 1U; b < count; ++b) {
            const auto &o = targets[b].rect;
            assert(!(r.x < o.x + o.width && o.x < r.x + r.width &&
                     r.y < o.y + o.height && o.y < r.y + r.height));
          }
        }
      }
    }
  }
}

int main() {
  test_target_contract();
  std::vector<uint8_t> regular_glyph_buffer(kStride * kHeight, 0xFFU);
  std::vector<uint8_t> medium_glyph_buffer(kStride * kHeight, 0xFFU);
  Canvas regular_glyph_canvas(kWidth, kHeight, regular_glyph_buffer.data(),
                              regular_glyph_buffer.size());
  Canvas medium_glyph_canvas(kWidth, kHeight, medium_glyph_buffer.data(),
                             medium_glyph_buffer.size());
  regular_glyph_canvas.draw_text_pixels(0, 0, "正文清晰", 18, GrayLevel::Black,
                                        FontWeight::Regular);
  medium_glyph_canvas.draw_text_pixels(0, 0, "正文清晰", 18, GrayLevel::Black,
                                       FontWeight::Medium);
  assert(count_level(regular_glyph_buffer, GrayLevel::Black) <
         count_level(medium_glyph_buffer, GrayLevel::Black));
  assert(count_level(regular_glyph_buffer, GrayLevel::DarkGray) == 0U);
  assert(count_level(regular_glyph_buffer, GrayLevel::LightGray) == 0U);

  std::vector<uint8_t> buffer(kStride * kHeight, 0xFFU);
  Canvas canvas(kWidth, kHeight, buffer.data(), buffer.size());

  pregnancy_page_render_clock_setup(canvas, "202609111530", false, false);
  assert(count_level(buffer, GrayLevel::Black) > 5000U);
  assert(pregnancy_page_action_at(PregnancyPage::ClockSetup, false, 542, 100) ==
         PregnancyAction::Digit1);
  assert(pregnancy_page_action_at(PregnancyPage::ClockSetup, false, 730, 330) ==
         PregnancyAction::Continue);
  assert(pregnancy_page_action_at(PregnancyPage::ClockSetup, false, 100, 420) ==
         PregnancyAction::None);
  assert(pregnancy_page_action_at(PregnancyPage::ClockSetup, true, 100, 420) ==
         PregnancyAction::Back);
  write_preview(buffer, "/tmp/pregnancy_clock_setup.ppm");

  pregnancy_page_render_due_date_setup(canvas, "20270318", true, true);
  assert(count_level(buffer, GrayLevel::Black) > 5000U);
  write_preview(buffer, "/tmp/pregnancy_due_date_setup.ppm");

  PregnancyProgress progress = {};
  progress.weeks = 20U;
  progress.days = 0U;
  progress.percent = 50U;
  progress.gestational_days = 140U;
  progress.days_until_due_date = 140;
  progress.stage = PregnancyStage::SecondTrimester;
  assert(font_text_width_pixels("正在感知外面的世界", 28U,
                                FontWeight::Medium) <= 360);
  assert(font_text_width_pixels("慢一点，也是在照顾宝宝", 28U,
                                FontWeight::Medium) <= 360);
  assert(font_text_width_pixels("有些人会逐渐感受到胎动，时间存在个体差异。",
                                16U, FontWeight::Regular) <= 360);
  for (uint8_t week = 6U; week <= 40U; ++week) {
    char sentence[160] = {};
    std::snprintf(sentence, sizeof(sentence), "%s。",
                  pregnancy_week_visual(week).sentence_zh);
    assert(font_text_width_pixels(sentence, 17U, FontWeight::Regular) <= 688);

    char size_title[80] = {};
    std::snprintf(size_title, sizeof(size_title), "宝宝现在约有%s那么大",
                  pregnancy_week_visual(week).object_zh);
    assert(font_text_width_pixels(size_title, 24U, FontWeight::Medium) <= 368 ||
           font_text_width_pixels(size_title, 20U, FontWeight::Medium) <= 368);

    const WeekContent week_content = pregnancy_content_for_week(week);
    assert(font_text_width_pixels(week_content.baby_development_zh, 16U,
                                  FontWeight::Regular) <= 544);
    assert(font_text_width_pixels(week_content.mother_changes_zh, 16U,
                                  FontWeight::Regular) <= 544);
    assert(font_text_width_pixels(week_content.daily_advice_zh, 16U,
                                  FontWeight::Regular) <= 544);
  }
  pregnancy_page_render_dashboard(canvas, {2027U, 3U, 18U}, progress);
  assert(count_level(buffer, GrayLevel::Black) > 8000U);
  assert(count_level(buffer, GrayLevel::DarkGray) == 0U);
  assert(count_level(buffer, GrayLevel::LightGray) == 0U);
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 750, 45) ==
         PregnancyAction::OpenLauncher);
  // Only visible controls are targets: the menu box and the three "this week"
  // rows. The illustration and the card header are not tappable.
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 736, 63) ==
         PregnancyAction::OpenLauncher);
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 160, 312) ==
         PregnancyAction::ShowBaby);
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 160, 368) ==
         PregnancyAction::ShowMom);
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 160, 424) ==
         PregnancyAction::ShowMom);
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 160, 240) ==
         PregnancyAction::None);
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 500, 250) ==
         PregnancyAction::None);
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 500, 350) ==
         PregnancyAction::None);
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 700, 468) ==
         PregnancyAction::None);
  write_preview(buffer, "/tmp/pregnancy_dashboard.ppm");

  ui_language_set(UiLanguage::ChineseSimplified);
  pregnancy_page_render_dashboard(canvas, {2027U, 3U, 18U}, progress);
  assert(count_level(buffer, GrayLevel::Black) > 8000U);
  write_preview(buffer, "/tmp/pregnancy_dashboard_zh.ppm");

  pregnancy_page_render_detail(canvas, PregnancyPage::Baby, progress,
                               pregnancy_content_for_week(progress.weeks));
  assert(pregnancy_page_action_at(PregnancyPage::Baby, true, 700, 67) ==
         PregnancyAction::ShowOverview);
  assert(pregnancy_page_action_at(PregnancyPage::Baby, true, 700, 68) ==
         PregnancyAction::None);
  write_canvas_preview(canvas, "/tmp/pregnancy_baby_landscape_zh.ppm");

  canvas.set_rotation(CanvasRotation::Deg90Clockwise);
  pregnancy_page_render_dashboard(canvas, {2027U, 3U, 18U}, progress);
  assert(canvas.width() == 480U);
  assert(canvas.height() == 800U);
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 480, 800, 440,
                                  40) == PregnancyAction::OpenLauncher);
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 480, 800, 416,
                                  63) == PregnancyAction::OpenLauncher);
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 480, 800, 240,
                                  652) == PregnancyAction::ShowBaby);
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 480, 800, 240,
                                  700) == PregnancyAction::ShowMom);
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 480, 800, 240,
                                  748) == PregnancyAction::ShowMom);
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 480, 800, 240,
                                  360) == PregnancyAction::None);
  const char *portrait_countdown = "距离预产期还有 140 天";
  const int portrait_countdown_width =
      font_text_width_pixels(portrait_countdown, 16U, FontWeight::Regular);
  const int portrait_heart_x = (480 - (portrait_countdown_width + 8 + 24)) / 2 +
                               portrait_countdown_width + 8;
  assert(count_black_rect(canvas, portrait_heart_x, 171, 24, 24) > 20U);
  write_canvas_preview(canvas, "/tmp/pregnancy_dashboard_portrait_zh.ppm");

  pregnancy_page_render_source_setup(canvas, true);
  assert(pregnancy_page_action_at(PregnancyPage::SourceSetup, true, 480, 800,
                                  240, 220) == PregnancyAction::SelectDueDate);
  assert(pregnancy_page_action_at(PregnancyPage::SourceSetup, true, 480, 800,
                                  240, 450) == PregnancyAction::SelectLmp);
  assert(pregnancy_page_action_at(PregnancyPage::SourceSetup, true, 480, 800,
                                  240, 700) == PregnancyAction::Back);
  write_canvas_preview(canvas, "/tmp/pregnancy_source_portrait_zh.ppm");

  pregnancy_page_render_clock_setup(canvas, "202609111530", false, true);
  assert(pregnancy_page_action_at(PregnancyPage::ClockSetup, true, 480, 800, 90,
                                  310) == PregnancyAction::Digit1);
  assert(pregnancy_page_action_at(PregnancyPage::ClockSetup, true, 480, 800,
                                  390, 580) == PregnancyAction::Continue);
  write_canvas_preview(canvas, "/tmp/pregnancy_setup_portrait_zh.ppm");

  pregnancy_page_render_detail(canvas, PregnancyPage::Baby, progress,
                               pregnancy_content_for_week(progress.weeks));
  assert(pregnancy_page_action_at(PregnancyPage::Baby, true, 480, 800, 240,
                                  752) == PregnancyAction::ShowBaby);
  write_canvas_preview(canvas, "/tmp/pregnancy_baby_portrait_zh.ppm");

  char digit = '\0';
  assert(pregnancy_action_digit(PregnancyAction::Digit0, digit));
  assert(digit == '0');
  assert(pregnancy_action_digit(PregnancyAction::Digit9, digit));
  assert(digit == '9');
  assert(!pregnancy_action_digit(PregnancyAction::Delete, digit));
  return 0;
}
