#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

#include "canvas.h"
#include "font.h"
#include "pregnancy_pages.h"
#include "pregnancy_plate_assets.h"
#include "ui_tokens.h"
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
        PregnancyTarget targets[kMaxPregnancyTargets];
        const size_t count = pregnancy_page_targets(
            page, can_cancel, size[0], size[1], targets, kMaxPregnancyTargets);
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

void test_key_policy() {
  assert(pregnancy_page_key_policy(PregnancyPage::Dashboard) == KeyPolicy::Home);
  assert(pregnancy_page_key_policy(PregnancyPage::Baby) == KeyPolicy::Free);
  assert(pregnancy_page_key_policy(PregnancyPage::Mom) == KeyPolicy::Free);
  for (PregnancyPage page : {PregnancyPage::ClockSetup, PregnancyPage::SourceSetup,
                             PregnancyPage::DateSetup})
    assert(pregnancy_page_key_policy(page) == KeyPolicy::Wizard);
}

int main() {
  test_key_policy();
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

  DigitFields clock_fields(DigitFieldKind::DateTime);
  clock_fields.set_digits("202609111530");
  clock_fields.select_field(3U);
  pregnancy_page_render_clock_setup(canvas, clock_fields, false, false);
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

  DigitFields due_fields(DigitFieldKind::Date);
  due_fields.set_digits("20270318");
  pregnancy_page_render_due_date_setup(canvas, due_fields, true, true);
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
  // Every week has a 340 px plate and an English caption that fits the page.
  for (uint8_t week = 6U; week <= 40U; ++week) {
    {
      const PixelAsset &plate = pregnancy_plate_asset(week);
      assert(plate.width == 340U && plate.height == 340U && plate.data != nullptr);
      // No plate may run into its own border: that would mean the object is cropped.
      const size_t stride = (static_cast<size_t>(plate.width) + 7U) / 8U;
      int min_x = plate.width, min_y = plate.height, max_x = -1, max_y = -1;
      for (int y = 0; y < plate.height; ++y)
        for (int x = 0; x < plate.width; ++x)
          if (plate.data[static_cast<size_t>(y) * stride + x / 8] & (1U << (7 - x % 8))) {
            min_x = std::min(min_x, x); max_x = std::max(max_x, x);
            min_y = std::min(min_y, y); max_y = std::max(max_y, y);
          }
      assert(max_x >= 0);
      assert(min_x >= 8 && min_y >= 8);
      assert(plate.width - 1 - max_x >= 8 && plate.height - 1 - max_y >= 8);
    }
    const char *caption = pregnancy_plate_caption(week);
    assert(caption != nullptr && caption[0] != '\0');
    int caption_width = 0;
    char one[2] = {0, 0};
    for (const char *c = caption; *c != '\0'; ++c) {
      one[0] = *c;
      caption_width += font_text_width_pixels(one, ui_tokens::kBody,
                                              FontWeight::Medium) + 6;
    }
    assert(caption_width - 6 <= 432);
  }
  {
    const PlateNumeral &numeral = pregnancy_plate_numeral();
    for (int digit = 0; digit < 10; ++digit) {
      assert(numeral.digit[digit].height == numeral.height);
      assert(numeral.advance[digit] >= numeral.digit[digit].width);
    }
  }
  pregnancy_page_render_dashboard(canvas, {2027U, 3U, 18U}, progress);
  assert(count_level(buffer, GrayLevel::Black) > 8000U);
  assert(count_level(buffer, GrayLevel::DarkGray) == 0U);
  assert(count_level(buffer, GrayLevel::LightGray) == 0U);
  // Targets: the menu icon, the engraving (Baby page) and the numeral block (Mom
  // page). The caption and the empty margins are not tappable.
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 750, 45) ==
         PregnancyAction::OpenLauncher);
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 580, 250) ==
         PregnancyAction::ShowBaby);
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 100, 150) ==
         PregnancyAction::ShowMom);
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 580, 440) ==
         PregnancyAction::None); // the caption is not tappable
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 100, 330) ==
         PregnancyAction::None); // the week grid is display only
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 340, 250) ==
         PregnancyAction::None);
  assert(count_black_rect(canvas, 410, 80, 340, 340) > 6000U);
  // Week 20: twenty finished cells are solid in the grid.
  assert(count_black_rect(canvas, 48, 252, 252, 156) > 20U * 28U * 28U * 8U / 10U);
  write_preview(buffer, "/tmp/pregnancy_dashboard.ppm");

  ui_language_set(UiLanguage::ChineseSimplified);
  pregnancy_page_render_dashboard(canvas, {2027U, 3U, 18U}, progress);
  assert(count_level(buffer, GrayLevel::Black) > 8000U);
  // The Chinese object name (not the English one) is centred under the picture, and the unit label under
  // the numeral switches from WEEKS to 孕周: both areas must carry ink and differ from the English frame.
  assert(count_black_rect(canvas, 410, 420, 340, 50) > 100U);
  const size_t zh_caption_ink = count_black_rect(canvas, 410, 420, 340, 50);
  const size_t zh_label_ink = count_black_rect(canvas, 30, 190, 260, 40);
  ui_language_set(UiLanguage::English);
  pregnancy_page_render_dashboard(canvas, {2027U, 3U, 18U}, progress);
  assert(count_black_rect(canvas, 410, 420, 340, 50) != zh_caption_ink);
  assert(count_black_rect(canvas, 30, 190, 260, 40) != zh_label_ink);
  ui_language_set(UiLanguage::ChineseSimplified);
  pregnancy_page_render_dashboard(canvas, {2027U, 3U, 18U}, progress);
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
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 480, 800, 428,
                                  52) == PregnancyAction::OpenLauncher);
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 480, 800, 240,
                                  300) == PregnancyAction::ShowBaby);
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 480, 800, 400,
                                  720) == PregnancyAction::ShowMom);
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 480, 800, 100,
                                  700) == PregnancyAction::None); // grid
  assert(pregnancy_page_action_at(PregnancyPage::Dashboard, true, 480, 800, 240,
                                  560) == PregnancyAction::None);
  assert(count_black_rect(canvas, 70, 130, 340, 340) > 6000U);
  assert(count_black_rect(canvas, 24, 630, 236, 146) > 20U * 26U * 26U * 8U / 10U);
  write_canvas_preview(canvas, "/tmp/pregnancy_dashboard_portrait_zh.ppm");

  pregnancy_page_render_source_setup(canvas, true);
  assert(pregnancy_page_action_at(PregnancyPage::SourceSetup, true, 480, 800,
                                  240, 220) == PregnancyAction::SelectDueDate);
  assert(pregnancy_page_action_at(PregnancyPage::SourceSetup, true, 480, 800,
                                  240, 450) == PregnancyAction::SelectLmp);
  assert(pregnancy_page_action_at(PregnancyPage::SourceSetup, true, 480, 800,
                                  240, 700) == PregnancyAction::Back);
  write_canvas_preview(canvas, "/tmp/pregnancy_source_portrait_zh.ppm");

  pregnancy_page_render_clock_setup(canvas, clock_fields, false, true);
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
