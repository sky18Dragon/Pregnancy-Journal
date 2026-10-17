// Host-side UI preview renderer.
//
// Compiles the real page renderers and hit-test functions (no hardware, no
// ESP-IDF) and, for every page x orientation x language, writes:
//   <name>.mono   logical-size bytes, 0 = black, 255 = white, after the same
//                 gray->1-bit threshold the firmware applies before sending
//                 to the SSD1677 (gray level 0/1 -> black, 2/3 -> white)
//   <name>.meta   size, count of pre-threshold gray pixels, action names, and
//                 the hit rectangles recovered from <page>_action_at()
// tools/ui_preview/render_png.py turns these into PNGs.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "app_pages.h"
#include "canvas.h"
#include "kick_pages.h"
#include "pregnancy_pages.h"
#include "press_feedback.h"
#include "prototype_ui.h"
#include "settings_pages.h"
#include "text_layout.h"
#include "ui_language.h"
#include "weight_pages.h"

namespace {

struct Case {
  std::string id;
  std::function<void(Canvas &)> render;
  // Returns an index into `names`, or -1 when the point hits nothing.
  std::function<int(int, int, int, int)> hit;
  std::vector<std::string> names;
};

std::vector<Case> g_cases;

// ---- sample data -----------------------------------------------------------

PregnancyProgress sample_progress() {
  PregnancyProgress progress = {};
  pregnancy_progress_calculate({2026U, 10U, 10U}, PregnancyDate{2027U, 2U, 20U},
                               progress);
  return progress;
}

KickDaySummary kick_today() {
  KickDaySummary v = {};
  v.morning = 12U; v.afternoon = 8U; v.evening = 15U; v.total = 35U;
  v.session_count = 4U; v.duration_minutes = 46U; v.latest_count = 9U;
  v.latest_duration_minutes = 12U; v.latest_hour = 18U; v.latest_minute = 26U;
  v.has_latest = true;
  return v;
}

KickTrendSummary kick_trend() {
  KickTrendSummary v = {};
  const uint16_t totals[kKickTrendDays] = {24U, 30U, 0U, 28U, 35U, 31U, 35U};
  for (size_t i = 0U; i < kKickTrendDays; ++i) {
    v.dates[i] = {2026U, 10U, static_cast<uint8_t>(4U + i)};
    v.totals[i] = totals[i];
  }
  v.period_totals[0] = 66U; v.period_totals[1] = 50U; v.period_totals[2] = 67U;
  v.total = 183U; v.session_count = 19U; v.average_per_recorded_day = 31U;
  v.recorded_days = 6U; v.busiest_period = KickPeriod::Evening; v.has_data = true;
  return v;
}

KickComparison kick_ready_timed() {
  KickComparison v = {};
  v.status = KickComparisonStatus::Ready; v.mode = KickMode::Timed;
  v.period = KickPeriod::Evening; v.current = 7U; v.baseline = 14U;
  v.difference = 7U; v.percent = 50U; v.percent_known = true;
  v.direction = KickDirection::Less; v.baseline_sessions = 5U;
  return v;
}

KickComparison kick_ready_ten() {
  KickComparison v = {};
  v.status = KickComparisonStatus::Ready; v.mode = KickMode::CountToTen;
  v.current = 1080U; v.baseline = 690U; v.difference = 390U; v.percent = 57U;
  v.percent_known = true; v.direction = KickDirection::More;
  v.baseline_sessions = 4U;
  return v;
}

KickAwarenessSummary kick_awareness() {
  KickAwarenessSummary v = {};
  v.usual = 4U; v.weaker = 1U; v.total = 5U;
  return v;
}

WeightService &weight_service() {
  static WeightService service;
  static bool filled = false;
  if (!filled) {
    filled = true;
    for (uint8_t day = 1U; day <= 10U; ++day) {
      WeightRecord r = {};
      r.date = {2026U, 10U, day};
      r.weight_tenths_kg = static_cast<uint16_t>(620U + day * 3U);
      service.upsert(r);
    }
  }
  return service;
}

// ---- case registration -----------------------------------------------------

#define NAMES(...) std::vector<std::string>{__VA_ARGS__}

void add(const std::string &id, std::function<void(Canvas &)> render,
         std::function<int(int, int, int, int)> hit,
         std::vector<std::string> names) {
  g_cases.push_back({id, std::move(render), std::move(hit), std::move(names)});
}

void register_launcher() {
  add("launcher", [](Canvas &c) { app_page_render_launcher(c, StickyAppId::Pregnancy); },
      [](int w, int h, int x, int y) {
        if (app_page_launcher_close_at(w, h, x, y)) return 0;
        StickyAppId app = StickyAppId::Pregnancy;
        if (app_page_launcher_app_at(w, h, x, y, app)) return 1 + static_cast<int>(app);
        return -1;
      },
      // Index 0 is Close; 1 + StickyAppId value (Settings=1 ... Kicks=8).
      NAMES("Close", "App:?0", "App:Settings", "App:Pregnancy", "App:Reminder",
            "App:Checkup", "App:?5", "App:?6", "App:Weight", "App:Kicks"));
}

void draw_standin_battery(Canvas &c) {
  int x = 0, y = 0;
  pregnancy_page_status_battery_origin(c.height() > c.width(), x, y);
  c.draw_rect(x, y, 44, 22, GrayLevel::Black);
  c.draw_rect(x + 1, y + 1, 42, 20, GrayLevel::Black);
  c.fill_rect(x + 44, y + 7, 4, 8, GrayLevel::Black);
  c.fill_rect(x + 4, y + 4, 36 * 70 / 100, 14, GrayLevel::Black);   // sample: 70 % full
}

void register_pregnancy() {
  static const std::vector<std::string> names = {
      "None", "Digit0", "Digit1", "Digit2", "Digit3", "Digit4", "Digit5",
      "Digit6", "Digit7", "Digit8", "Digit9", "Delete", "Continue", "Back",
      "SelectDueDate", "SelectLmp", "ShowOverview", "ShowBaby",
      "ShowMom", "OpenLauncher", "Field0", "Field1", "Field2", "Field3", "Field4"};
  auto hit = [](PregnancyPage page, bool cancel) {
    return [page, cancel](int w, int h, int x, int y) {
      const int a = static_cast<int>(pregnancy_page_action_at(page, cancel, w, h, x, y));
      return a == 0 ? -1 : a;
    };
  };
  add("pregnancy_clock_setup",
      [](Canvas &c) {
        DigitFields fields(DigitFieldKind::DateTime);
        fields.set_digits("202610101530");
        fields.select_field(3U); // editing the hour
        pregnancy_page_render_clock_setup(c, fields, false, true);
      },
      hit(PregnancyPage::ClockSetup, true), names);
  add("pregnancy_source_setup",
      [](Canvas &c) { pregnancy_page_render_source_setup(c, true); },
      hit(PregnancyPage::SourceSetup, true), names);
  add("pregnancy_date_setup",
      [](Canvas &c) {
        DigitFields fields(DigitFieldKind::Date);
        fields.set_digits("20270220");
        pregnancy_page_render_profile_date_setup(c, fields, false, true);
      },
      hit(PregnancyPage::DateSetup, true), names);
  add("pregnancy_dashboard",
      [](Canvas &c) { pregnancy_page_render_dashboard(c, {2027U, 2U, 20U}, sample_progress()); },
      hit(PregnancyPage::Dashboard, false), names);
  // Status row. The real indicator needs the battery driver, so a stand-in with the same footprint
  // (44x22 battery with nub, no label) is drawn at the origin the page reports.
  add("pregnancy_dashboard_status",
      [](Canvas &c) {
        pregnancy_page_render_dashboard(c, {2027U, 2U, 20U}, sample_progress());
        draw_standin_battery(c);
      },
      hit(PregnancyPage::Dashboard, false), names);
  add("pregnancy_dashboard_sleep",
      [](Canvas &c) {
        pregnancy_page_render_dashboard(c, {2027U, 2U, 20U}, sample_progress());
        draw_standin_battery(c);
        pregnancy_page_render_sleep_moon(c);
      },
      hit(PregnancyPage::Dashboard, false), names);
  add("pregnancy_baby",
      [](Canvas &c) {
        const PregnancyProgress p = sample_progress();
        pregnancy_page_render_detail(c, PregnancyPage::Baby, p, pregnancy_content_for_week(p.weeks));
      },
      hit(PregnancyPage::Baby, false), names);
  add("pregnancy_mom",
      [](Canvas &c) {
        const PregnancyProgress p = sample_progress();
        pregnancy_page_render_detail(c, PregnancyPage::Mom, p, pregnancy_content_for_week(p.weeks));
      },
      hit(PregnancyPage::Mom, false), names);
}

void register_weight() {
  static const std::vector<std::string> names = {
      "None", "Back", "ToggleUnit", "ToggleTrend", "AddToday", "History",
      "Profile", "SelectRecord", "Previous", "Next", "MinusOne", "MinusTenth",
      "PlusTenth", "PlusOne", "Save", "Delete", "HeightDown", "HeightUp",
      "BaselineDown", "BaselineUp", "Confirm", "Cancel", "HeightDownBig",
      "HeightUpBig", "BaselineDownBig", "BaselineUpBig"};
  struct Entry { const char *id; WeightPage page; };
  const Entry entries[] = {
      {"weight_dashboard", WeightPage::Dashboard}, {"weight_history", WeightPage::History},
      {"weight_editor", WeightPage::Editor}, {"weight_profile", WeightPage::Profile},
      {"weight_replace_confirm", WeightPage::ReplaceConfirm},
      {"weight_delete_confirm", WeightPage::DeleteConfirm}};
  for (const Entry &entry : entries) {
    const WeightPage page = entry.page;
    add(entry.id,
        [page](Canvas &c) {
          WeightPageState state = {};
          state.page = page;
          state.today = {2026U, 10U, 10U};
          state.edit_date = state.today;
          state.edit_id = 1U;
          state.edit_display_weight = 650U;
          state.profile = {165U, 640U, false};
          weight_page_render(c, weight_service(), {165U, 640U, false}, state);
        },
        [page](int w, int h, int x, int y) {
          const int a = static_cast<int>(weight_page_action_at(page, w, h, x, y).action);
          return a == 0 ? -1 : a;
        },
        names);
  }
}

void register_kicks() {
  static const std::vector<std::string> names = {
      "None", "Back", "Start", "ShowInsights", "Count", "Undo", "Finish",
      "StartTimed", "StartCountToTen", "StartAwareness", "FeelUsual",
      "FeelWeaker", "FeelFewer", "FeelChanged", "ShowCompare", "Done",
      "ChangeMethod"};
  auto hit = [](KickPage page) {
    return [page](int w, int h, int x, int y) {
      const int a = static_cast<int>(kick_page_action_at(page, w, h, x, y));
      return a == 0 ? -1 : a;
    };
  };
  add("kick_today", [](Canvas &c) { kick_page_render_today(c, kick_today()); },
      hit(KickPage::Today), names);
  add("kick_today_method",
      [](Canvas &c) {
        kick_page_render_today(c, kick_today(), false, {true, KickMode::CountToTen});
      },
      hit(KickPage::Today), names);
  add("kick_today_checkin",
      [](Canvas &c) {
        kick_page_render_today(c, kick_today(), true, {true, KickMode::Awareness});
      },
      hit(KickPage::Today), names);
  add("kick_insights",
      [](Canvas &c) { kick_page_render_insights(c, kick_today(), kick_trend()); },
      hit(KickPage::Insights), names);
  add("kick_mode_select", [](Canvas &c) { kick_page_render_mode_select(c); },
      hit(KickPage::ModeSelect), names);
  add("kick_mode_select_set",
      [](Canvas &c) { kick_page_render_mode_select(c, {true, KickMode::CountToTen}); },
      hit(KickPage::ModeSelect), names);
  add("kick_counter_timed",
      [](Canvas &c) { kick_page_render_counter(c, KickPeriod::Evening, 7U, 5U, KickMode::Timed); },
      hit(KickPage::Counter), names);
  add("kick_counter_ten",
      [](Canvas &c) { kick_page_render_counter(c, KickPeriod::Evening, 7U, 5U, KickMode::CountToTen); },
      hit(KickPage::Counter), names);
  add("kick_feel", [](Canvas &c) { kick_page_render_feel(c); }, hit(KickPage::Feel), names);
  add("kick_result",
      [](Canvas &c) {
        KickSession s = {};
        s.hour = 20U; s.minute = 5U; s.count = 7U; s.elapsed_seconds = 3600U;
        s.mode = KickMode::Timed;
        kick_page_render_result(c, s, kick_ready_timed(), kick_awareness());
      },
      hit(KickPage::Result), names);
  add("kick_compare",
      [](Canvas &c) {
        const KickComparison periods[3] = {kick_ready_timed(), {}, kick_ready_timed()};
        const bool has[3] = {true, false, true};
        kick_page_render_compare(c, periods, has, true, kick_ready_ten(), true,
                                 kick_awareness());
      },
      hit(KickPage::Compare), names);
}

void register_settings() {
  static const std::vector<std::string> names = {
      "None", "ToggleLanguage", "EditTime", "RefreshDisplay", "PregnancySettings",
      "StorageSettings", "ShortcutSettings", "ShortcutWeight",
      "ShortcutBabyWeek", "ShortcutKick", "ShortcutOff", "BeginStorageInitialization", "SafeEjectStorage",
      "ConfirmStorageInitialization", "CancelStorageInitialization",
      "ManageStorage", "ManageLegacyCommon", "ManageLegacyKicks",
      "BeginLegacyImport", "BeginLegacyClear", "ConfirmLegacyClear",
      "CancelLegacyClear", "BeginCardDataDelete", "ConfirmCardDataDelete",
      "CancelCardDataDelete", "BeginDeviceReset", "ConfirmDeviceReset",
      "CancelDeviceReset", "EditPregnancy", "Home", "Back", "Delete", "Save",
      "Digit0", "Digit1", "Digit2", "Digit3", "Digit4", "Digit5", "Digit6",
      "Digit7", "Digit8", "Digit9", "Field0", "Field1", "Field2", "Field3",
      "Field4"};
  auto hit = [](SettingsPage page) {
    return [page](int w, int h, int x, int y) {
      const int a = static_cast<int>(settings_page_action_at(page, w, h, x, y, true, true));
      return a == 0 ? -1 : a;
    };
  };
  auto storage = [] {
    SettingsStoragePageState s = {};
    s.card = SettingsStorageCardView::ReadWrite;
    s.dataset = SettingsStorageDatasetView::Ready;
    s.requests_open = true; s.eject_enabled = true; s.migration_enabled = true;
    s.delete_enabled = true; s.capacity_known = true; s.total_mib = 30708U;
    s.free_mib = 2936U;
    std::strcpy(s.dataset_id_prefix, "00b454dc");
    s.dataset_generation = 2U;
    return s;
  };
  add("settings_main",
      [](Canvas &c) { settings_page_render_main(c, true, "2026.10.10 14:42", "2027.02.20"); },
      hit(SettingsPage::Main), names);
  add("settings_main_shortcut_kick",
      [](Canvas &c) {
        settings_page_render_main(c, true, "2026.10.10 14:42", "2027.02.20", false,
                                  StickyShortcutTarget::KickStart);
      },
      hit(SettingsPage::Main), names);
  add("settings_shortcut",
      [](Canvas &c) {
        settings_page_render_shortcut(c, StickyShortcutTarget::WeightRecordToday);
      },
      hit(SettingsPage::Shortcut), names);
  add("settings_shortcut_kick",
      [](Canvas &c) { settings_page_render_shortcut(c, StickyShortcutTarget::KickStart); },
      hit(SettingsPage::Shortcut), names);
  add("settings_time_editor",
      [](Canvas &c) {
        DigitFields fields(DigitFieldKind::DateTime);
        fields.set_digits("202610101442");
        fields.select_field(4U); // editing the minute
        settings_page_render_time_editor(c, fields, false);
      },
      hit(SettingsPage::TimeEditor), names);
  add("settings_pregnancy",
      [](Canvas &c) {
        PregnancyProfile profile = {};
        pregnancy_profile_from_due_date({2027U, 2U, 20U}, profile);
        settings_page_render_pregnancy(c, profile, sample_progress(), true);
      },
      hit(SettingsPage::PregnancySummary), names);
  add("settings_storage",
      [storage](Canvas &c) { settings_page_render_storage(c, storage()); },
      hit(SettingsPage::StorageSummary), names);
  add("settings_storage_confirm",
      [](Canvas &c) { settings_page_render_storage_confirm(c); },
      hit(SettingsPage::StorageConfirm), names);
  add("settings_storage_more",
      [storage](Canvas &c) { settings_page_render_storage_more(c, storage()); },
      hit(SettingsPage::StorageMore), names);
  add("settings_legacy_migration",
      [](Canvas &c) {
        SettingsMigrationPageState s = {};
        s.view = SettingsMigrationView::Verified;
        s.valid = 12U; s.imported = 12U; s.import_enabled = false; s.clear_enabled = true;
        settings_page_render_legacy_migration(c, s);
      },
      hit(SettingsPage::LegacyMigration), names);
  add("settings_legacy_clear_confirm",
      [](Canvas &c) { settings_page_render_legacy_clear_confirm(c, false); },
      hit(SettingsPage::LegacyClearConfirm), names);
  add("settings_card_delete_confirm",
      [](Canvas &c) { settings_page_render_card_data_delete_confirm(c, "00b454dc"); },
      hit(SettingsPage::CardDataDeleteConfirm), names);
  add("settings_device_reset_confirm",
      [](Canvas &c) { settings_page_render_device_reset_confirm(c); },
      hit(SettingsPage::DeviceResetConfirm), names);
}

// ---- text lint hook --------------------------------------------------------

struct TextItem { int x, y, width, size; std::string text; };
std::vector<TextItem> g_texts;

std::set<std::string> g_missing;
void observe_missing(const char *english) {
  // Already-Chinese text that only embeds an identifier (a dataset id such as "00b454dc") is data, not a gap.
  for (const char *c = english; *c != '\0'; ++c)
    if (static_cast<unsigned char>(*c) >= 0xE3) return;
  for (const char *c = english; *c != '\0'; ++c)
    if ((*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z')) { g_missing.insert(english); return; }
}

std::vector<std::string> g_truncated;
void observe_truncation(const char *text) { g_truncated.push_back(text); }

void observe_text(int x, int y, int width, int size, const char *text) {
  g_texts.push_back({x, y, width, size, text});
}

// ---- output ----------------------------------------------------------------

struct Rect { int id, x0, y0, x1, y1; };

std::vector<Rect> hit_rects(const std::vector<int> &map, int w, int h) {
  std::vector<Rect> rects;
  std::vector<uint8_t> seen(map.size(), 0U);
  std::vector<int> stack;
  for (int start = 0; start < w * h; ++start) {
    if (map[start] < 0 || seen[start]) continue;
    Rect r = {map[start], w, h, 0, 0};
    stack.assign(1, start);
    seen[start] = 1U;
    while (!stack.empty()) {
      const int p = stack.back();
      stack.pop_back();
      const int x = p % w, y = p / w;
      if (x < r.x0) r.x0 = x;
      if (y < r.y0) r.y0 = y;
      if (x > r.x1) r.x1 = x;
      if (y > r.y1) r.y1 = y;
      const int nx[4] = {x - 1, x + 1, x, x};
      const int ny[4] = {y, y, y - 1, y + 1};
      for (int i = 0; i < 4; ++i) {
        if (nx[i] < 0 || ny[i] < 0 || nx[i] >= w || ny[i] >= h) continue;
        const int q = ny[i] * w + nx[i];
        if (!seen[q] && map[q] == r.id) { seen[q] = 1U; stack.push_back(q); }
      }
    }
    rects.push_back(r);
  }
  return rects;
}

void emit(const Case &c, Canvas &canvas, const char *orientation,
          const char *language, const std::string &dir) {
  const int w = canvas.width(), h = canvas.height();
  const std::string base = dir + "/" + c.id + "_" + orientation + "_" + language;

  // Pre-threshold statistics: gray pixels never survive on the real panel.
  size_t dark = 0U, light = 0U;
  std::vector<uint8_t> mono(static_cast<size_t>(w) * h);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const GrayLevel g = canvas.pixel_at(x, y);
      if (g == GrayLevel::DarkGray) ++dark;
      if (g == GrayLevel::LightGray) ++light;
      mono[static_cast<size_t>(y) * w + x] = static_cast<uint8_t>(g) >= 2U ? 255U : 0U;
    }
  if (FILE *f = std::fopen((base + ".mono").c_str(), "wb")) {
    std::fwrite(mono.data(), 1U, mono.size(), f);
    std::fclose(f);
  }

  std::vector<int> map(static_cast<size_t>(w) * h, -1);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const int id = c.hit(w, h, x, y);
      map[static_cast<size_t>(y) * w + x] =
          (id >= 0 && id < static_cast<int>(c.names.size())) ? id : (id >= 0 ? 999 : -1);
    }
  const std::vector<Rect> rects = hit_rects(map, w, h);
  if (FILE *f = std::fopen((base + ".meta").c_str(), "w")) {
    std::fprintf(f, "size %d %d\ngray_dark %zu\ngray_light %zu\n", w, h, dark, light);
    for (const Rect &r : rects) {
      const char *name = r.id < static_cast<int>(c.names.size()) ? c.names[r.id].c_str() : "?";
      std::fprintf(f, "hit %s %d %d %d %d\n", name, r.x0, r.y0, r.x1 - r.x0 + 1,
                   r.y1 - r.y0 + 1);
    }
    for (const std::string &t : g_truncated)
      std::fprintf(f, "truncated %s\n", t.c_str());
    for (const TextItem &t : g_texts) {
      std::string flat = t.text;
      for (char &ch : flat) if (ch == '\n' || ch == '\r') ch = ' ';
      std::fprintf(f, "text %d %d %d %d %s\n", t.size, t.x, t.y, t.width, flat.c_str());
    }
    std::fclose(f);
  }

  // Every hit target drawn in its pressed state (what the tap feedback shows).
  // This mutates the canvas, so it must stay the last step.
  for (const Rect &r : rects)
    press_feedback::draw(canvas, {r.x0, r.y0, r.x1 - r.x0 + 1, r.y1 - r.y0 + 1});
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      mono[static_cast<size_t>(y) * w + x] =
          static_cast<uint8_t>(canvas.pixel_at(x, y)) >= 2U ? 255U : 0U;
  if (FILE *f = std::fopen((base + ".pressed.raw").c_str(), "wb")) {
    std::fwrite(mono.data(), 1U, mono.size(), f);
    std::fclose(f);
  }
}

} // namespace

int main(int argc, char **argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  const std::string filter = argc > 2 ? argv[2] : "";
  register_launcher();
  register_pregnancy();
  register_weight();
  register_kicks();
  register_settings();

  std::vector<uint8_t> pixels(800U * 480U / 4U);
  Canvas canvas(800U, 480U, pixels.data(), pixels.size());
  int count = 0;
  for (const Case &c : g_cases) {
    if (!filter.empty() && c.id.find(filter) == std::string::npos) continue;
    for (int lang = 0; lang < 2; ++lang) {
      ui_language_set(lang == 0 ? UiLanguage::English : UiLanguage::ChineseSimplified);
      for (int portrait = 0; portrait < 2; ++portrait) {
        canvas.set_rotation(portrait ? CanvasRotation::Deg90Clockwise : CanvasRotation::Deg0);
        g_texts.clear();
        g_truncated.clear();
        canvas_set_text_observer(observe_text);
        ui_set_truncation_observer(observe_truncation);
        ui_set_missing_translation_observer(lang == 1 ? observe_missing : nullptr);
        c.render(canvas);
        ui_set_missing_translation_observer(nullptr);
        canvas_set_text_observer(nullptr);
        ui_set_truncation_observer(nullptr);
        emit(c, canvas, portrait ? "portrait" : "landscape", lang == 0 ? "en" : "zh", dir);
        ++count;
      }
    }
  }
  if (FILE *f = std::fopen((dir + "/missing_translations.txt").c_str(), "w")) {
    for (const std::string &key : g_missing) std::fprintf(f, "%s\n", key.c_str());
    std::fclose(f);
  }
  std::printf("rendered %d previews into %s (%zu untranslated strings)\n", count,
              dir.c_str(), g_missing.size());
  return 0;
}
