// Home-page design lab (host only, no hardware, no ESP-IDF).
//
// Renders candidate layouts for the pregnancy home page through the REAL
// firmware pieces: Canvas, the CJK font strikes, ui_tokens, prototype_ui
// primitives and the 148x148 week-size illustrations. Each concept is drawn
// for every requested week x orientation and written as
//   <concept>_w<NN>_<landscape|portrait>_zh.mono / .meta
// in the same format tools/ui_preview/render_png.py already understands, so the
// existing lint (font size, overlap, 48 px targets, gray pixels) applies.
//
// A concept is a plain function; add one by writing `render_xxx` and listing it
// in kConcepts. Geometry rule from CLAUDE.md: the rectangles a concept draws
// are the rectangles it reports in `Targets`, so the hit overlay is honest.
//
// This is a design sandbox. Nothing here is linked into firmware.
// 首页设计实验室：只用于宿主机预览，不进入固件。
#include <algorithm>
#include <cstdarg>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <cctype>

#include "canvas.h"
#include "chinese_font_assets.h"
#include "font.h"
#include "pixel_asset.h"
#include "pregnancy_week_assets.h"
#include "pregnancy_week_visual.h"
#include "content_service.h"
#include "prototype_ui.h"
#include "text_layout.h"
#include "ui_language.h"
#include "ui_tokens.h"
#include "display_assets.inc"

namespace {

using prototype_ui::Rect;
namespace tok = ui_tokens;

// ---- inputs ----------------------------------------------------------------

struct Input {
  int week;       // completed weeks (6..41)
  int day;        // 0..6
  int percent;    // 0..100, capped at the 40-week nominal term
  int days_left;  // days to the due date; negative when overdue
  bool overdue;
};

struct Facts {
  const char *object;   // familiar-object name, e.g. 胡萝卜
  double length_cm;
  int weight_g;
  int asset_week;       // week used for the illustration (clamped 6..40)
  std::string growth;   // this week's one-line development note
  std::string english;  // upper-case caption
};

struct Targets {
  Rect menu = {0, 0, 0, 0};
  Rect baby = {0, 0, 0, 0};
  Rect mom = {0, 0, 0, 0};
};

// ---- week size reference (placeholder data, see the TSV header) -------------

struct SizeRow { int week; double length; int weight; };
std::vector<SizeRow> g_sizes;

bool load_sizes(const std::string &path) {
  std::ifstream in(path);
  if (!in) return false;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream row(line);
    SizeRow r = {};
    if (row >> r.week >> r.length >> r.weight) g_sizes.push_back(r);
  }
  return !g_sizes.empty();
}

void size_for_week(int week, double &length, int &weight) {
  if (g_sizes.empty()) { length = 0; weight = 0; return; }
  if (week <= g_sizes.front().week) { length = g_sizes.front().length; weight = g_sizes.front().weight; return; }
  if (week >= g_sizes.back().week) { length = g_sizes.back().length; weight = g_sizes.back().weight; return; }
  for (size_t i = 1; i < g_sizes.size(); ++i) {
    if (week <= g_sizes[i].week) {
      const SizeRow &a = g_sizes[i - 1], &b = g_sizes[i];
      const double t = static_cast<double>(week - a.week) / (b.week - a.week);
      length = a.length + (b.length - a.length) * t;
      weight = static_cast<int>(a.weight + (b.weight - a.weight) * t + 0.5);
      return;
    }
  }
}

// Optional per-week labels (week, english, chinese) from the art folder, so the
// caption matches the illustration actually shown.
struct Label { std::string en, zh; };
std::map<int, Label> g_labels;

bool load_labels(const std::string &path) {
  std::ifstream in(path);
  if (!in) return false;
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream row(line);
    int week = 0;
    std::string en, zh;
    if (std::getline(row, en, '\t') && !en.empty()) {
      week = std::atoi(en.c_str());
      std::getline(row, en, '\t');
      std::getline(row, zh, '\t');
      g_labels[week] = {en, zh};
    }
  }
  return !g_labels.empty();
}

Input make_input(int week) {
  Input in = {};
  in.week = week;
  in.day = week >= 40 ? 0 : 3;
  const int days = week * 7 + in.day;
  in.percent = std::min(100, days * 100 / 280);
  in.days_left = 280 - days;
  in.overdue = days > 280;
  return in;
}

Facts make_facts(const Input &in) {
  Facts f = {};
  f.asset_week = std::max(6, std::min(40, in.week));
  f.object = pregnancy_week_visual(static_cast<uint8_t>(f.asset_week)).object_zh;
  const auto label = g_labels.find(f.asset_week);
  if (label != g_labels.end()) f.object = label->second.zh.c_str();
  size_for_week(f.asset_week, f.length_cm, f.weight_g);
  const WeekContent c = pregnancy_content_for_week(f.asset_week);
  f.growth = c.baby_development_zh != nullptr ? c.baby_development_zh : "";
  f.english = label != g_labels.end() ? label->second.en : std::string();
  return f;
}

// ---- small drawing helpers --------------------------------------------------

constexpr GrayLevel kInk = GrayLevel::Black;

std::string fmt(const char *format, ...) __attribute__((format(printf, 1, 2)));
std::string fmt(const char *format, ...) {
  char buffer[512] = {};
  va_list args;
  va_start(args, format);
  std::vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  return buffer;
}

int text_width(const std::string &s, int px, FontWeight w) {
  return font_text_width_pixels(s.c_str(), static_cast<uint8_t>(px), w);
}

void text(Canvas &c, int x, int y, const std::string &s, int px,
          FontWeight w = FontWeight::Regular, GrayLevel color = kInk) {
  c.draw_text_pixels(x, y, s.c_str(), static_cast<uint8_t>(px), color, w);
}

void text_centered(Canvas &c, const Rect &r, const std::string &s, int px,
                   FontWeight w = FontWeight::Regular,
                   GrayLevel color = kInk) {
  text(c, r.x + (r.width - text_width(s, px, w)) / 2,
       r.y + (r.height - px) / 2, s, px, w, color);
}

// Week illustration. Packed illustration art (HOME_LAB_HAVE_ART) is drawn 1:1 at 280 px for
// `scale` >= 2 and at 150 px for scale 1; otherwise the firmware pixel art.
bool g_use_art = true;

void fruit(Canvas &c, int cx, int cy, const Facts &f, int scale) {
#if HOME_LAB_HAVE_ART
  if (g_use_art) {
    const PixelAsset &art = (scale >= 2 ? kArtBig : kArtSmall)[f.asset_week - 6];
    if (art.data != nullptr) {
      pixel_asset_draw_centered(c, cx, cy, art, 1, kInk);
      return;
    }
  }
#endif
  pixel_asset_draw_centered(c, cx, cy, pregnancy_week_size_asset(
                                           static_cast<uint8_t>(f.asset_week)),
                            static_cast<uint8_t>(scale), kInk);
}

const char *trimester(const Input &in) {
  return in.week < 14 ? "孕早期" : (in.week < 28 ? "孕中期" : "孕晚期");
}

std::string countdown(const Input &in) {
  return in.overdue ? fmt("已超过预产期 %d 天", -in.days_left)
                    : fmt("距离预产期还有 %d 天", in.days_left);
}

std::string countdown_short(const Input &in) {
  return in.overdue ? fmt("已超过预产期 %d 天", -in.days_left)
                    : fmt("距预产期 %d 天", in.days_left);
}

std::string length_fact(const Facts &f) {
  return f.length_cm < 10 ? fmt("身长约 %.1f cm", f.length_cm)
                          : fmt("身长约 %.0f cm", f.length_cm);
}

// ---- poster typography ---------------------------------------------------------
// Numerals and the 孕周 label are pre-rendered serif bitmaps (see
// make_display_assets.py); everything else uses the firmware font.

int numeral_width(const std::string &s, const NumeralFont &f) {
  int w = 0;
  const int gap = f.advance[0] - f.digit[0].width;
  for (char ch : s) w += f.digit[ch - '0'].width + gap;
  return w - gap;
}

void numeral(Canvas &c, int x, int y, const std::string &s,
             const NumeralFont &f) {
  for (char ch : s) {
    if (ch < '0' || ch > '9') continue;
    pixel_asset_draw(c, x, y, f.digit[ch - '0'], 1, kInk);
    x += f.advance[ch - '0'];
  }
}

std::string week_digits(const Input &in) { return fmt("%d", in.week); }

// Quiet menu: three strokes, no frame. The hit rectangle is still 56 px.
void menu_icon(Canvas &c, const Rect &r) {
  for (int line = 0; line < 3; ++line)
    c.fill_rect(r.x + 16, r.y + 16 + line * 10, 24, 2, kInk);
}

// Text link: label, chevron, and a 2 px rule along the bottom of the hit area.
void text_link(Canvas &c, const Rect &r, const char *label) {
  const int tw = text_width(label, tok::kBody, FontWeight::Medium);
  text(c, r.x, r.y + 12, label, tok::kBody, FontWeight::Medium);
  prototype_ui::draw_chevron(c, r.x + tw + 12, r.y + 15);
  c.fill_rect(r.x, r.y + r.height - 6, r.width, 2, kInk);
}

std::string lead_in(const Facts &f) { return fmt("宝宝约有%s那么大", f.object); }

std::string upper(const char *s) {
  std::string out = s;
  for (char &ch : out) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  return out;
}

constexpr double kPi = 3.14159265358979323846;

// ---- concept E: medallion --------------------------------------------------------
// A dial of 40 week-dots frames the object like a museum medallion. Solid dot
// = a finished week, halo = the week in progress, small dot = still to come.

void week_dial(Canvas &c, int cx, int cy, int radius, const Input &in) {
  for (int k = 28; k <= 30; ++k) c.draw_circle(cx, cy, radius - k, kInk);
  for (int w = 1; w <= 40; ++w) {
    const double a = -kPi / 2 + (w - 0.5) * 2 * kPi / 40;
    const int x = cx + static_cast<int>(std::lround(radius * std::cos(a)));
    const int y = cy + static_cast<int>(std::lround(radius * std::sin(a)));
    if (w <= in.week) {
      c.fill_circle(x, y, 6, kInk);
    } else if (w == in.week + 1) {
      c.fill_circle(x, y, 8, kInk);
      c.draw_circle(x, y, 14, kInk);
      c.draw_circle(x, y, 13, kInk);
    } else {
      c.fill_circle(x, y, 3, kInk);
    }
  }
}

void render_medallion(Canvas &c, const Input &in, const Facts &f, Targets &t,
                      bool portrait) {
  const std::string digits = week_digits(in);
  const std::string sentence = lead_in(f);
  if (!portrait) {
    t.menu = {720, 24, 56, 56};
    pixel_asset_draw(c, 36, 40, kLabelWeek, 1, kInk);
    numeral(c, 32, 84, digits, kNumeralXl);
    text(c, 36, 308, sentence, tok::kCardTitle, FontWeight::Medium);
    text(c, 36, 342, fmt("%s · %s", trimester(in), countdown(in).c_str()),
         tok::kBody);
    t.baby = {36, 408, 120, 48};
    t.mom = {172, 408, 120, 48};
    week_dial(c, 576, 240, 184, in);
    fruit(c, 576, 240, f, 2);
  } else {
    t.menu = {400, 24, 56, 56};
    pixel_asset_draw(c, 24, 36, kLabelWeek, 1, kInk);
    numeral(c, 24, 78, digits, kNumeralLg);
    text(c, 252, 92, trimester(in), tok::kCardTitle, FontWeight::Medium);
    text(c, 252, 132, in.overdue ? "已超过预产期" : "距预产期", tok::kBody);
    text(c, 252, 160, fmt("%d 天", in.overdue ? -in.days_left : in.days_left),
         tok::kSectionHeading, FontWeight::Medium);
    text(c, 24, 252, sentence, tok::kCardTitle, FontWeight::Medium);
    t.baby = {24, 728, 120, 48};
    t.mom = {160, 728, 120, 48};
    week_dial(c, 240, 508, 176, in);
    fruit(c, 240, 508, f, 2);
  }
  menu_icon(c, t.menu);
  text_link(c, t.baby, "宝宝");
  text_link(c, t.mom, "妈妈");
}

// ---- concept F: rings of time -----------------------------------------------------
// Seven nested, slightly irregular contours around the week number, like tree
// rings. Solid rings = how far the pregnancy has come, dashed = still to grow.
// The contour phases shift with the week, so the shape is never quite the same.

void organic_ring(Canvas &c, int cx, int cy, double radius, double phase,
                  bool dashed) {
  constexpr int kPoints = 720;
  for (int i = 0; i < kPoints; ++i) {
    if (dashed && (i / 9) % 2 == 1) continue;
    const double a = 2 * kPi * i / kPoints;
    const double r = radius * (1 + 0.030 * std::sin(2 * a + phase) +
                               0.020 * std::sin(3 * a + phase * 1.7 + 1.0) +
                               0.010 * std::sin(5 * a + phase * 2.3 + 2.0));
    c.fill_circle(cx + static_cast<int>(std::lround(r * std::cos(a))),
                  cy + static_cast<int>(std::lround(r * std::sin(a))), 1, kInk);
  }
}

void ring_nest(Canvas &c, int cx, int cy, const Input &in, double inner,
               double step) {
  const int solid = std::max(1, (in.percent * 7 + 99) / 100);
  for (int k = 0; k < 7; ++k)
    organic_ring(c, cx, cy, inner + step * k, in.week * 0.31 + k * 0.28, k >= solid);
}

void ring_numeral(Canvas &c, int cx, int cy, const Input &in) {
  const std::string digits = week_digits(in);
  const int w = numeral_width(digits, kNumeralMd);
  numeral(c, cx - w / 2, cy - 88, digits, kNumeralMd);
  pixel_asset_draw(c, cx - kLabelWeek.width / 2, cy + 42, kLabelWeek, 1, kInk);
}

void render_rings(Canvas &c, const Input &in, const Facts &f, Targets &t,
                  bool portrait) {
  const std::string sentence = lead_in(f);
  if (!portrait) {
    t.menu = {720, 24, 56, 56};
    ring_nest(c, 250, 240, in, 108, 16);
    ring_numeral(c, 250, 240, in);
    fruit(c, 606, 182, f, 2);
    text_centered(c, {456, 336, 300, 24}, sentence, tok::kCardTitle, FontWeight::Medium);
    text_centered(c, {456, 368, 300, 18},
                  fmt("%s · %s", trimester(in), countdown_short(in).c_str()), tok::kBody);
    t.baby = {488, 408, 120, 48};
    t.mom = {628, 408, 120, 48};
  } else {
    t.menu = {400, 24, 56, 56};
    ring_nest(c, 240, 290, in, 108, 16);
    ring_numeral(c, 240, 290, in);
    fruit(c, 98, 612, f, 1);
    text(c, 192, 560, sentence, tok::kCardTitle, FontWeight::Medium);
    text(c, 192, 598, trimester(in), tok::kBody);
    text(c, 192, 626, countdown_short(in), tok::kBody);
    t.baby = {24, 728, 120, 48};
    t.mom = {160, 728, 120, 48};
  }
  menu_icon(c, t.menu);
  text_link(c, t.baby, "宝宝");
  text_link(c, t.mom, "妈妈");
}

// ---- concept G: specimen plate ------------------------------------------------------
// A museum-plate layout: hairline header, oversized numeral, the object as the
// specimen, and a ruler of 40 week ticks along the foot.

void week_ruler(Canvas &c, int x0, int baseline, int pitch, const Input &in,
                int label_y) {
  for (int w = 1; w <= 40; ++w) {
    const int x = x0 + (w - 1) * pitch;
    const int h = w <= in.week ? 30 : (w == in.week + 1 ? 46 : 14);
    c.fill_rect(x, baseline - h, 2, h, kInk);
  }
  for (int w : {14, 28, 40}) {
    const std::string label = fmt("%d", w);
    const int x = x0 + (w - 1) * pitch + 1;
    text(c, x - text_width(label, tok::kSecondaryBody, FontWeight::Regular) / 2,
         label_y, label, tok::kSecondaryBody);
  }
  c.fill_rect(x0 - 8, baseline, (39 * pitch) + 18, 2, kInk);
}

void render_specimen(Canvas &c, const Input &in, const Facts &f, Targets &t,
                     bool portrait) {
  const std::string digits = week_digits(in);
  const std::string sentence = lead_in(f);
  const std::string english = upper(pregnancy_week_visual(
      static_cast<uint8_t>(f.asset_week)).object_en);
  if (!portrait) {
    t.menu = {720, 24, 56, 56};
    t.baby = {470, 28, 116, 48};
    t.mom = {598, 28, 116, 48};
    text(c, 24, 42, fmt("%s · %s", trimester(in), countdown(in).c_str()), tok::kBody);
    c.fill_rect(24, 88, 752, 2, kInk);
    numeral(c, 32, 102, digits, kNumeralXl);
    pixel_asset_draw(c, 32 + numeral_width(digits, kNumeralXl) + 14, 102 + 200 - kLabelWeek.height,
                     kLabelWeek, 1, kInk);
    text(c, 36, 316, sentence, tok::kCardTitle, FontWeight::Medium);
    text(c, 36, 350, english, tok::kSecondaryBody);
    text(c, 36, 372, length_fact(f), tok::kBody);
    fruit(c, 590, 244, f, 2);
    week_ruler(c, 40, 432, 18, in, 438);
  } else {
    t.menu = {400, 24, 56, 56};
    t.baby = {252, 112, 150, 48};
    t.mom = {252, 172, 150, 48};
    text(c, 24, 42, trimester(in), tok::kCardTitle, FontWeight::Medium);
    c.fill_rect(24, 88, 432, 2, kInk);
    numeral(c, 24, 108, digits, kNumeralLg);
    pixel_asset_draw(c, 24, 108 + 160 + 14, kLabelWeek, 1, kInk);
    text(c, 252, 252, countdown_short(in), tok::kBody);
    fruit(c, 240, 452, f, 2);
    text_centered(c, {24, 612, 432, 24}, sentence, tok::kCardTitle, FontWeight::Medium);
    text_centered(c, {24, 646, 432, 16}, english, tok::kSecondaryBody);
    text_centered(c, {24, 670, 432, 18}, length_fact(f), tok::kBody);
    week_ruler(c, 40, 736, 10, in, 744);
  }
  menu_icon(c, t.menu);
  text_link(c, t.baby, "宝宝");
  text_link(c, t.mom, "妈妈");
}

// ---- concept H: riso poster -----------------------------------------------------
// Whole-page redesign after a risograph fruit-and-veg print series: a heavy
// condensed English title ("CARROT //"), flat object shapes whose leaves overprint
// the fruit (overlap flips to paper), a big offset black disc behind the hero that
// the object crosses (ink XOR), calendar numerals, and a 40-tick ruler as progress.

int title_index(char ch) {
  if (ch >= 'A' && ch <= 'Z') return ch - 'A';
  return ch == '/' ? 26 : 27;
}

int title_width(const std::string &s, const TitleFont &f) {
  int w = 0;
  for (char ch : s) w += f.advance[title_index(ch)];
  return w;
}

void title(Canvas &c, int x, int y, const std::string &s, const TitleFont &f) {
  for (char ch : s) {
    const int i = title_index(ch);
    if (f.glyph[i].data != nullptr) pixel_asset_draw(c, x, y, f.glyph[i], 1, kInk);
    x += f.advance[i];
  }
}

const TitleFont &title_fit(const std::string &s, int max_width) {
  if (title_width(s, kTitleLg) <= max_width) return kTitleLg;
  if (title_width(s, kTitleMd) <= max_width) return kTitleMd;
  return kTitleSm;
}

// Draws an ink-only asset by flipping the pixels underneath (black <-> white).
void xor_asset(Canvas &c, int cx, int cy, const PixelAsset &a) {
  if (a.data == nullptr) return;
  const int x0 = cx - a.width / 2, y0 = cy - a.height / 2;
  const size_t stride = (static_cast<size_t>(a.width) + 7U) / 8U;
  for (int y = 0; y < a.height; ++y)
    for (int x = 0; x < a.width; ++x)
      if (a.data[static_cast<size_t>(y) * stride + x / 8] & (1U << (7 - x % 8)))
        c.draw_pixel(x0 + x, y0 + y,
                     c.pixel_at(x0 + x, y0 + y) == GrayLevel::Black ? GrayLevel::White
                                                                    : GrayLevel::Black);
}

void hero_with_disc(Canvas &c, int disc_x, int disc_y, int disc_r, int art_x,
                    int art_y, const Facts &f) {
  c.fill_circle(disc_x, disc_y, disc_r, kInk);
#if HOME_LAB_HAVE_ART
  if (g_use_art && kArtBig[f.asset_week - 6].data != nullptr) {
    xor_asset(c, art_x, art_y, kArtBig[f.asset_week - 6]);
    return;
  }
#endif
  fruit(c, art_x, art_y, f, 2);
}

void render_riso_poster(Canvas &c, const Input &in, const Facts &f, Targets &t,
                        bool portrait) {
  const std::string name = upper(pregnancy_week_visual(
      static_cast<uint8_t>(f.asset_week)).object_en) + " //";
  const std::string digits = week_digits(in);
  const std::string sentence = lead_in(f);
  if (!portrait) {
    t.menu = {720, 24, 56, 56};
    t.baby = {470, 28, 116, 48};
    t.mom = {598, 28, 116, 48};
    title(c, 24, 24, name, title_fit(name, 420));
    hero_with_disc(c, 620, 232, 128, 584, 250, f);
    numeral(c, 28, 104, digits, kNumeralXl);
    pixel_asset_draw(c, 28 + numeral_width(digits, kNumeralXl) + 14, 104 + 200 - kLabelWeek.height,
                     kLabelWeek, 1, kInk);
    text(c, 28, 318, sentence, tok::kCardTitle, FontWeight::Medium);
    text(c, 28, 350, fmt("%s · %s", trimester(in), countdown(in).c_str()), tok::kBody);
    week_ruler(c, 40, 432, 18, in, 438);
  } else {
    t.menu = {400, 24, 56, 56};
    t.baby = {24, 102, 120, 48};
    t.mom = {160, 102, 120, 48};
    title(c, 24, 24, name, title_fit(name, 364));
    hero_with_disc(c, 292, 330, 140, 236, 346, f);
    numeral(c, 24, 516, digits, kNumeralLg);
    const int nx = 24 + numeral_width(digits, kNumeralLg) + 22;
    pixel_asset_draw(c, nx, 520, kLabelWeek, 1, kInk);
    ui_draw_wrapped_text_pixels(c, nx, 562, 456 - nx, sentence.c_str(), tok::kBody, kInk, 2,
                                tok::kBodyLineGap, FontWeight::Medium);
    text(c, nx, 614, trimester(in), tok::kBody);
    text(c, nx, 640, countdown_short(in), tok::kBody);
    week_ruler(c, 40, 748, 10, in, 754);
  }
  menu_icon(c, t.menu);
  text_link(c, t.baby, "宝宝");
  text_link(c, t.mom, "妈妈");
}

// ---- concept I: plate ------------------------------------------------------------
// After the engraved week-by-week plate sheet: a big calendar numeral with WEEKS
// under it, the engraving as the hero, a letter-spaced English caption, and an
// 8 x 5 contact-sheet map of the 40 weeks as the progress indicator.

int tracked_width(const std::string &s, int px, int track, FontWeight w) {
  int total = 0;
  for (char ch : s) total += text_width(std::string(1, ch), px, w) + track;
  return total - track;
}

void text_tracked(Canvas &c, int x, int y, const std::string &s, int px, int track,
                  FontWeight w = FontWeight::Medium) {
  for (char ch : s) {
    const std::string one(1, ch);
    text(c, x, y, one, px, w);
    x += text_width(one, px, w) + track;
  }
}

void week_map(Canvas &c, int x0, int y0, int cell, int pitch, const Input &in) {
  for (int i = 1; i <= 40; ++i) {
    const int col = (i - 1) % 8, row = (i - 1) / 8;
    const Rect r = {x0 + col * pitch, y0 + row * pitch, cell, cell};
    if (i <= in.week) {
      c.fill_rect(r.x, r.y, r.width, r.height, kInk);
    } else if (i == in.week + 1) {
      prototype_ui::draw_frame(c, r, tok::kEmphasisStroke);
      c.fill_rect(r.x + 8, r.y + 8, r.width - 16, r.height - 16, kInk);
    } else {
      prototype_ui::draw_frame(c, r, tok::kBorderStroke);
    }
  }
}

void plate_hero(Canvas &c, int cx, int cy, const Facts &f) {
#if HOME_LAB_HAVE_ART
  if (g_use_art && kArtBig[f.asset_week - 6].data != nullptr) {
    pixel_asset_draw_centered(c, cx, cy, kArtBig[f.asset_week - 6], 1, kInk);
    return;
  }
#endif
  fruit(c, cx, cy, f, 2);
}

void render_plate(Canvas &c, const Input &in, const Facts &f, Targets &t,
                  bool portrait) {
  const std::string digits = week_digits(in);
  const std::string caption = f.english.empty()
      ? upper(pregnancy_week_visual(static_cast<uint8_t>(f.asset_week)).object_en)
      : f.english;
  const int cap_w = tracked_width(caption, tok::kBody, 7, FontWeight::Medium);
  const std::string sentence = lead_in(f);
  if (!portrait) {
    t.menu = {720, 24, 56, 56};
    t.baby = {470, 28, 116, 48};
    t.mom = {598, 28, 116, 48};
    numeral(c, 28, 28, digits, kNumeralLg);
    text_tracked(c, 32, 198, "WEEKS", tok::kBody, 7);
    text(c, 28, 238, sentence, tok::kCardTitle, FontWeight::Medium);
    text(c, 28, 272, fmt("%s · %s", trimester(in), countdown(in).c_str()), tok::kBody);
    week_map(c, 28, 316, 24, 28, in);
    plate_hero(c, 570, 238, f);
    text_tracked(c, 570 - cap_w / 2, 398, caption, tok::kBody, 7);
  } else {
    t.menu = {400, 24, 56, 56};
    t.baby = {24, 200, 120, 48};
    t.mom = {160, 200, 120, 48};
    numeral(c, 24, 24, digits, kNumeralLg);
    const int nx = 24 + numeral_width(digits, kNumeralLg) + 22;
    text_tracked(c, nx, 34, "WEEKS", tok::kBody, 7);
    ui_draw_wrapped_text_pixels(c, nx, 92, 456 - nx, sentence.c_str(), tok::kBody, kInk, 2,
                                tok::kBodyLineGap, FontWeight::Medium);
    text(c, nx, 144, trimester(in), tok::kBody);
    text(c, nx, 170, countdown_short(in), tok::kBody);
    plate_hero(c, 240, 404, f);
    text_tracked(c, 240 - cap_w / 2, 562, caption, tok::kBody, 7);
    week_map(c, 24, 604, 26, 32, in);
    text(c, 298, 612, in.overdue ? "已超过预产期" : "距预产期", tok::kBody);
    const std::string days = fmt("%d", in.overdue ? -in.days_left : in.days_left);
    text(c, 298, 642, days, tok::kDisplay, FontWeight::Medium);
    text(c, 298 + text_width(days, tok::kDisplay, FontWeight::Medium) + 8, 672, "天",
         tok::kSectionHeading, FontWeight::Medium);
  }
  menu_icon(c, t.menu);
  text_link(c, t.baby, "宝宝");
  text_link(c, t.mom, "妈妈");
}

// ---- concept J: plate, exactly like the reference cell ---------------------------
// One cell of the engraved week sheet and nothing else: big condensed numeral top
// left, WEEKS under it, the engraving centred, a letter-spaced English caption at
// the foot, plus the quiet menu stroke. Tap the picture for the Baby page and the
// numeral block for the Mom page (no visible buttons, as in the reference).

void render_exact(Canvas &c, const Input &in, const Facts &f, Targets &t,
                  bool portrait) {
  const std::string digits = week_digits(in);
  const std::string caption = f.english.empty()
      ? upper(pregnancy_week_visual(static_cast<uint8_t>(f.asset_week)).object_en)
      : f.english;
  const int cap_w = tracked_width(caption, tok::kBody, 6, FontWeight::Medium);
  const NumeralFont &num = portrait ? kNumeralPlateLg : kNumeralPlateMd;
  const int hero_w =
#if HOME_LAB_HAVE_ART
      kArtBig[f.asset_week - 6].width;
#else
      296;
#endif
  const int cx = portrait ? 240 : 400;
  const int cy = portrait ? 430 : 236;
  numeral(c, 48, portrait ? 56 : 44, digits, num);
  text_tracked(c, 50, (portrait ? 56 : 44) + num.height + 12, "WEEKS", tok::kEmphasis, 3);
  plate_hero(c, cx, cy, f);
  text_tracked(c, cx - cap_w / 2, portrait ? 690 : 424, caption, tok::kBody, 6);
  t.menu = portrait ? Rect{400, 24, 56, 56} : Rect{720, 24, 56, 56};
  t.baby = {cx - hero_w / 2, cy - hero_w / 2, hero_w, hero_w};
  t.mom = {36, (portrait ? 56 : 44) - 12, 200, num.height + 12 + tok::kEmphasis + 24};
  menu_icon(c, t.menu);
}

// ---- registry ----------------------------------------------------------------

struct Concept {
  const char *id;
  void (*render)(Canvas &, const Input &, const Facts &, Targets &, bool);
};

const Concept kConcepts[] = {
    {"e_medallion", render_medallion},
    {"f_rings", render_rings},
    {"g_specimen", render_specimen},
    {"h_riso", render_riso_poster},
    {"i_plate", render_plate},
    {"j_exact", render_exact},
};

// ---- lint hooks + output -------------------------------------------------------

struct TextItem { int x, y, width, size; std::string text; };
std::vector<TextItem> g_texts;
std::vector<std::string> g_truncated;

void observe_text(int x, int y, int width, int size, const char *s) {
  g_texts.push_back({x, y, width, size, s});
}
void observe_truncation(const char *s) { g_truncated.push_back(s); }

// A glyph the committed font strikes do not contain would draw as the
// fallback box on the device; report it instead of letting it slip through.
std::string missing_glyphs(const std::string &s, int px) {
  std::string missing;
  for (FontWeight w : {FontWeight::Regular, FontWeight::Medium}) {
    const ChineseFontStrike &strike =
        chinese_font_strike(static_cast<uint8_t>(px), w);
    const char *p = s.c_str();
    uint32_t cp = 0;
    while (*p != '\0' && font_decode_utf8(p, cp)) {
      if (cp < 0x80) continue;
      if (chinese_font_glyph(strike, cp) == nullptr) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "U+%04X", cp);
        if (missing.find(buf) == std::string::npos) missing += std::string(missing.empty() ? "" : ",") + buf;
      }
    }
  }
  return missing;
}

void emit(Canvas &canvas, const std::string &base, const Targets &t) {
  const int w = canvas.width(), h = canvas.height();
  size_t dark = 0, light = 0;
  std::vector<uint8_t> mono(static_cast<size_t>(w) * h);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const GrayLevel g = canvas.pixel_at(x, y);
      if (g == GrayLevel::DarkGray) ++dark;
      if (g == GrayLevel::LightGray) ++light;
      mono[static_cast<size_t>(y) * w + x] =
          static_cast<uint8_t>(g) >= 2U ? 255U : 0U;
    }
  if (FILE *f = std::fopen((base + ".mono").c_str(), "wb")) {
    std::fwrite(mono.data(), 1U, mono.size(), f);
    std::fclose(f);
  }
  if (FILE *f = std::fopen((base + ".meta").c_str(), "w")) {
    std::fprintf(f, "size %d %d\ngray_dark %zu\ngray_light %zu\n", w, h, dark, light);
    struct Named { const char *name; const Rect *r; };
    for (const Named &n : {Named{"Menu", &t.menu}, Named{"Baby", &t.baby}, Named{"Mom", &t.mom}})
      if (n.r->width > 0)
        std::fprintf(f, "hit %s %d %d %d %d\n", n.name, n.r->x, n.r->y, n.r->width, n.r->height);
    for (const std::string &s : g_truncated) std::fprintf(f, "truncated %s\n", s.c_str());
    for (const TextItem &item : g_texts) {
      std::string flat = item.text;
      for (char &ch : flat) if (ch == '\n' || ch == '\r') ch = ' ';
      std::fprintf(f, "text %d %d %d %d %s\n", item.size, item.x, item.y, item.width, flat.c_str());
      const std::string missing = missing_glyphs(flat, item.size);
      if (!missing.empty())
        std::fprintf(f, "missing_glyph %s %s\n", missing.c_str(), flat.c_str());
    }
    std::fclose(f);
  }
}

std::vector<int> parse_weeks(const std::string &spec) {
  std::vector<int> weeks;
  if (spec == "all") {
    for (int w = 6; w <= 41; ++w) weeks.push_back(w);
    return weeks;
  }
  std::istringstream in(spec);
  std::string token;
  while (std::getline(in, token, ',')) {
    const int w = std::atoi(token.c_str());
    if (w >= 6 && w <= 41) weeks.push_back(w);
  }
  return weeks;
}

} // namespace

// usage: home_lab <out_dir> <size_reference.tsv> <weeks: all|6,12,21> [concept_filter]
int main(int argc, char **argv) {
  if (argc < 4) {
    std::fprintf(stderr, "usage: %s <out_dir> <size_reference.tsv> <weeks|all> [concept]\n", argv[0]);
    return 2;
  }
  const std::string dir = argv[1];
  if (!load_sizes(argv[2])) {
    std::fprintf(stderr, "cannot read size reference %s\n", argv[2]);
    return 2;
  }
  const std::vector<int> weeks = parse_weeks(argv[3]);
  const std::string filter = argc > 4 ? argv[4] : "";
  if (argc > 5 && std::string(argv[5]) == "pixel") g_use_art = false;
  if (argc > 6 && argv[6][0] != 0) load_labels(argv[6]);
  if (weeks.empty()) {
    std::fprintf(stderr, "no valid weeks in '%s' (6..41)\n", argv[3]);
    return 2;
  }

  ui_language_set(UiLanguage::ChineseSimplified);
  std::vector<uint8_t> pixels(800U * 480U / 4U);
  Canvas canvas(800U, 480U, pixels.data(), pixels.size());
  int count = 0;
  for (const Concept &concept : kConcepts) {
    if (!filter.empty() && std::string(concept.id).find(filter) == std::string::npos) continue;
    for (int week : weeks) {
      const Input in = make_input(week);
      const Facts facts = make_facts(in);
      for (int portrait = 0; portrait < 2; ++portrait) {
        canvas.set_rotation(portrait ? CanvasRotation::Deg90Clockwise : CanvasRotation::Deg0);
        canvas.clear(GrayLevel::White);
        g_texts.clear();
        g_truncated.clear();
        canvas_set_text_observer(observe_text);
        ui_set_truncation_observer(observe_truncation);
        Targets targets;
        concept.render(canvas, in, facts, targets, portrait != 0);
        canvas_set_text_observer(nullptr);
        ui_set_truncation_observer(nullptr);
        emit(canvas, fmt("%s/%s_w%02d_%s_zh", dir.c_str(), concept.id, week,
                         portrait ? "portrait" : "landscape"),
             targets);
        ++count;
      }
    }
  }
  std::printf("rendered %d lab frames into %s\n", count, dir.c_str());
  return 0;
}
