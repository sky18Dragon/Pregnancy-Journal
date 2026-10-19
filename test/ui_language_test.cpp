#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

#include "canvas.h"
#include "chinese_font_assets.h"
#include "ui_language.h"

int main() {
  ui_language_set(UiLanguage::English);
  assert(std::strcmp(ui_text("BACK"), "BACK") == 0);
  const int english_back_width = ui_text_width("BACK", 2);
  assert(english_back_width > 0);

  ui_language_set(UiLanguage::ChineseSimplified);
  assert(std::strcmp(ui_text("BACK"), "返回") == 0);
  assert(std::strcmp(ui_text("UNMAPPED"), "UNMAPPED") == 0);
  const int chinese_back_width = ui_text_width("BACK", 2);
  assert(chinese_back_width > 0);
  assert(chinese_back_width < english_back_width);
  assert(chinese_font_glyph_count() > 100U);

  // Status pages and operation results that no preview page draws: in Chinese mode none may stay English
  // or lack a glyph.
  const char *const unpreviewed[] = {
      "MICROSD CARD IS REQUIRED", "MICROSD CARD IS READ ONLY", "MICROSD STORAGE IS FULL",
      "CREATE OR SELECT A DATASET IN SETTINGS", "MICROSD DATA NEEDS REVIEW", "MICROSD CARD CHANGED",
      "MICROSD DATA CHANGED · RELOAD REQUIRED", "MICROSD STORAGE UNAVAILABLE", "MICROSD DATA",
      "PREGNANCY PROFILE STORAGE", "NO FLASH FALLBACK WAS USED", "RETRY",
      "CONFIRM DETAILS WITH YOUR CARE TEAM", "SAFE TO REMOVE CARD", "SAFE EJECT FAILED",
      "PREGJNL DATA DELETED", "APP DATA DELETE FAILED",
      "SAVE FAILED - RETRYING", "CAPACITY UNAVAILABLE", "STORAGE UNAVAILABLE", "DATASET ID UNAVAILABLE",
      "MICROSD ERROR", "LEGACY KICK DATA IS PRESENT AND WILL BE PRESERVED.",
  };
  for (const char *english : unpreviewed) {
    const char *shown = ui_text(english);
    assert(std::strcmp(shown, english) != 0);
    for (const char *c = shown; *c != '\0'; ++c)
      assert(!((*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z')));
    assert(ui_text_width(english, 2) > 0);
  }

  std::vector<uint8_t> pixels((120U * 40U + 3U) / 4U, 0xFFU);
  Canvas canvas(120, 40, pixels.data(), pixels.size());
  canvas.draw_text(4, 4, "BACK", 2, GrayLevel::Black);
  size_t black_pixels = 0U;
  for (int y = 0; y < 40; ++y) {
    for (int x = 0; x < 120; ++x) {
      black_pixels += canvas.pixel_at(x, y) == GrayLevel::Black ? 1U : 0U;
    }
  }
  assert(black_pixels > 20U);
  return 0;
}
