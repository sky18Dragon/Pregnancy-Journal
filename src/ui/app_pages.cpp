#include "app_pages.h"

#include "canvas.h"
#include "prototype_ui.h"
#include "text_layout.h"
#include "ui_language.h"
#include "ui_tokens.h"

namespace {

using prototype_ui::Rect;

struct Entry {
  StickyAppId app;
  const char *number;
  const char *title_en;
  const char *title_zh;
  const char *summary_en;
  const char *summary_zh;
};

constexpr Entry kEntries[] = {
    {StickyAppId::Pregnancy, "01", "Baby Week", "孕周",
     "This week's development", "查看本周发育与变化"},
    {StickyAppId::Weight, "02", "Weight", "体重",
     "Latest record and trend", "最近记录与长期趋势"},
    {StickyAppId::Kicks, "03", "Kicks", "胎动",
     "Today's sessions and trend", "早中晚三个时段记录"},
    {StickyAppId::Settings, "04", "Settings", "设置",
     "Device and pregnancy profile", "设备设置与孕期资料"},
};

constexpr Rect kCloseRect = {656, 20, 120, ui_tokens::kMinimumTouchHeight};

bool portrait_layout(int width, int height) { return height > width; }

Rect close_rect(int width, int height) {
  return portrait_layout(width, height)
             ? Rect{336, 20, 120, ui_tokens::kMinimumTouchHeight}
             : kCloseRect;
}

Rect entry_rect(size_t index, int width, int height) {
  if (portrait_layout(width, height))
    return {24, 108 + static_cast<int>(index) * 148, 432, 140};
  return {24 + static_cast<int>(index % 2U) * 388,
          100 + static_cast<int>(index / 2U) * 160, 364, 152};
}

} // namespace

void app_page_render_launcher(Canvas &canvas, StickyAppId current_app) {
  (void)current_app;
  prototype_ui::begin_page(canvas);
  canvas.draw_text_pixels(ui_tokens::kContentMargin, 20, ui_language_is_chinese() ? "功能" : "APPS",
                          ui_tokens::kPageHeading, GrayLevel::Black,
                          FontWeight::Medium);
  const bool portrait = canvas.height() > canvas.width();
  const Rect close = close_rect(canvas.width(), canvas.height());
  // The subtitle wraps clear of the close button instead of running under it.
  ui_draw_wrapped_text_pixels(
      canvas, ui_tokens::kContentMargin, 58, close.x - 12 - 24,
      ui_language_is_chinese()
          ? "记录、查看与管理你的孕期信息。"
          : "Record, review and manage pregnancy information.",
      ui_tokens::reading_size(), GrayLevel::Black, 2, 2, FontWeight::Regular);
  prototype_ui::draw_button(canvas, close,
                            ui_language_is_chinese() ? "关闭" : "CLOSE", false,
                            2);
  canvas.draw_line(24, 99, canvas.width() - 24, 99, GrayLevel::Black);

  for (size_t index = 0U; index < sizeof(kEntries) / sizeof(kEntries[0]);
       ++index) {
    const Entry &entry = kEntries[index];
    const Rect rect = entry_rect(index, canvas.width(), canvas.height());
    const bool chinese = ui_language_is_chinese();
    canvas.draw_text_pixels(rect.x, rect.y + (portrait ? 26 : 30), entry.number,
                            ui_tokens::kPageHeading, GrayLevel::Black,
                            FontWeight::Medium);
    canvas.draw_text_pixels(rect.x + 64, rect.y + (portrait ? 20 : 24),
                            chinese ? entry.title_zh : entry.title_en,
                            ui_tokens::kSectionHeading, GrayLevel::Black,
                            FontWeight::Medium);
    canvas.draw_text_pixels(rect.x + 64, rect.y + (portrait ? 54 : 58),
                            chinese ? entry.summary_zh : entry.summary_en,
                            ui_tokens::reading_size(), GrayLevel::Black,
                            FontWeight::Regular);
    prototype_ui::draw_chevron(canvas, rect.x + rect.width - 18,
                               rect.y + (portrait ? 38 : 44));
    canvas.draw_line(rect.x, rect.y + rect.height - 1, rect.x + rect.width,
                     rect.y + rect.height - 1, GrayLevel::Black);
  }
  ui_draw_wrapped_text_pixels(
      canvas, ui_tokens::kContentMargin, portrait ? 750 : 444, canvas.width() - 2 * ui_tokens::kContentMargin,
      ui_language_is_chinese()
          ? "顶部按键关闭 · 双击为快捷键"
          : "TOP: CLOSE / DOUBLE-CLICK: SHORTCUT",
      ui_tokens::reading_size(), GrayLevel::Black, 2, 2, FontWeight::Regular);
}

StickyAppId app_page_launcher_neighbor(StickyAppId current, int direction) {
  constexpr int kCount = static_cast<int>(sizeof(kEntries) / sizeof(kEntries[0]));
  int index = -1;
  for (int i = 0; i < kCount; ++i)
    if (kEntries[i].app == current) index = i;
  if (index < 0) return kEntries[0].app;
  const int step = direction < 0 ? kCount - 1 : 1;
  return kEntries[(index + step) % kCount].app;
}

size_t app_page_launcher_targets(int width, int height, LauncherTarget *out,
                                 size_t capacity) {
  size_t count = 0U;
  const auto add = [&](const Rect &rect, bool close, StickyAppId app) {
    if (out != nullptr && count < capacity) out[count] = {rect, close, app};
    ++count;
  };
  add(close_rect(width, height), true, StickyAppId::Pregnancy);
  for (size_t index = 0U; index < sizeof(kEntries) / sizeof(kEntries[0]);
       ++index)
    add(entry_rect(index, width, height), false, kEntries[index].app);
  return count < capacity ? count : capacity;
}

bool app_page_launcher_close_at(int width, int height, int x, int y) {
  LauncherTarget targets[8];
  const size_t count = app_page_launcher_targets(width, height, targets, 8U);
  for (size_t i = 0U; i < count; ++i)
    if (targets[i].rect.contains(x, y)) return targets[i].close;
  return false;
}

bool app_page_launcher_app_at(int width, int height, int x, int y,
                              StickyAppId &selected_app) {
  LauncherTarget targets[8];
  const size_t count = app_page_launcher_targets(width, height, targets, 8U);
  for (size_t i = 0U; i < count; ++i) {
    if (targets[i].rect.contains(x, y)) {
      if (targets[i].close) return false;
      selected_app = targets[i].app;
      return true;
    }
  }
  return false;
}
