#include "digit_fields.h"

#include <algorithm>
#include <cstring>

#include "font.h"
#include "ui_tokens.h"

namespace {

constexpr uint8_t kDateTimeWidths[] = {4U, 2U, 2U, 2U, 2U};
constexpr uint8_t kDateWidths[] = {4U, 2U, 2U};
// Separator drawn after field i ("" = just a gap).
constexpr const char *kDateTimeSeparators[] = {"-", "-", "", ":"};
constexpr const char *kDateSeparators[] = {"-", "-"};

constexpr int kCellPadding = 8;
constexpr int kSeparatorGap = 6;
constexpr int kCellInset = 6;

const uint8_t *widths_for(DigitFieldKind kind) {
  return kind == DigitFieldKind::DateTime ? kDateTimeWidths : kDateWidths;
}

const char *separator_after(DigitFieldKind kind, size_t index) {
  return kind == DigitFieldKind::DateTime ? kDateTimeSeparators[index]
                                          : kDateSeparators[index];
}

} // namespace

size_t DigitFields::field_count() const {
  return kind_ == DigitFieldKind::DateTime ? 5U : 3U;
}

size_t DigitFields::total() const {
  return kind_ == DigitFieldKind::DateTime ? 12U : 8U;
}

size_t DigitFields::field_width(size_t index) const {
  return index < field_count() ? widths_for(kind_)[index] : 0U;
}

size_t DigitFields::field_offset(size_t index) const {
  size_t offset = 0U;
  for (size_t i = 0U; i < index && i < field_count(); ++i)
    offset += widths_for(kind_)[i];
  return offset;
}

void DigitFields::begin(DigitFieldKind kind) {
  kind_ = kind;
  std::memset(digits_, '_', kMaxDigits);
  digits_[total()] = '\0';
  digits_[kMaxDigits] = '\0';
  active_ = 0U;
  cursor_ = 0U;
  armed_ = true;
}

bool DigitFields::set_digits(const char *digits) {
  begin(kind_);
  if (digits == nullptr || std::strlen(digits) != total()) return false;
  for (size_t i = 0U; i < total(); ++i)
    if (digits[i] < '0' || digits[i] > '9') return false;
  std::memcpy(digits_, digits, total());
  digits_[total()] = '\0';
  return true;
}

void DigitFields::select_field(size_t index) {
  if (index >= field_count()) return;
  active_ = index;
  cursor_ = 0U;
  armed_ = true;
}

void DigitFields::clear_field(size_t index) {
  std::memset(digits_ + field_offset(index), '_', field_width(index));
}

// Field roles in order: year, month, day, hour, minute.
bool DigitFields::digit_allowed(size_t field, size_t position,
                                char digit) const {
  const char *content = digits_ + field_offset(field);
  const char first = position == 0U ? digit : content[0];
  switch (field) {
  case 0U: // year 19xx / 20xx; the rest is checked on save
    if (position == 0U) return digit == '1' || digit == '2';
    if (position == 1U) return first == '1' ? digit == '9' : digit == '0';
    return true;
  case 1U: // month 01-12
    if (position == 0U) return digit == '0' || digit == '1';
    return first == '0' ? digit >= '1' : digit <= '2';
  case 2U: // day 01-31
    if (position == 0U) return digit <= '3';
    if (first == '0') return digit >= '1';
    return first == '3' ? digit <= '1' : true;
  case 3U: // hour 00-23
    if (position == 0U) return digit <= '2';
    return first == '2' ? digit <= '3' : true;
  default: // minute 00-59
    return position == 0U ? digit <= '5' : true;
  }
}

bool DigitFields::input_digit(char digit) {
  if (digit < '0' || digit > '9') return false;
  const size_t position = armed_ ? 0U : cursor_;
  if (position >= field_width(active_)) return false;
  // When armed the field's old content is about to be replaced, so judge the
  // digit as the first of a fresh entry.
  if (armed_) {
    if (!digit_allowed(active_, 0U, digit)) return false;
    clear_field(active_);
    armed_ = false;
    cursor_ = 0U;
  } else if (!digit_allowed(active_, cursor_, digit)) {
    return false;
  }
  digits_[field_offset(active_) + cursor_] = digit;
  ++cursor_;
  if (cursor_ >= field_width(active_)) {
    if (active_ + 1U < field_count())
      select_field(active_ + 1U);
    else {
      cursor_ = 0U;
      armed_ = true; // another digit restarts the last field
    }
  }
  return true;
}

void DigitFields::erase() {
  if (!armed_ && cursor_ > 0U) {
    --cursor_;
    digits_[field_offset(active_) + cursor_] = '_';
    return;
  }
  const char *content = digits_ + field_offset(active_);
  const bool blank = content[0] == '_';
  if (!blank) {
    clear_field(active_);
    armed_ = false;
    cursor_ = 0U;
    return;
  }
  if (active_ > 0U) select_field(active_ - 1U);
}

bool DigitFields::field_complete(size_t index) const {
  if (index >= field_count()) return false;
  const char *content = digits_ + field_offset(index);
  for (size_t i = 0U; i < field_width(index); ++i)
    if (content[i] == '_') return false;
  return true;
}

bool DigitFields::complete() const {
  for (size_t i = 0U; i < field_count(); ++i)
    if (!field_complete(i)) return false;
  return true;
}

size_t digit_field_cells(const prototype_ui::Rect &box, DigitFieldKind kind,
                         uint8_t pixel_size, prototype_ui::Rect *out,
                         size_t capacity) {
  const size_t count = kind == DigitFieldKind::DateTime ? 5U : 3U;
  const uint8_t *widths = widths_for(kind);
  int cell_widths[DigitFields::kMaxFields] = {};
  int total = 0;
  for (size_t i = 0U; i < count; ++i) {
    char sample[5] = "0000";
    sample[widths[i]] = '\0';
    cell_widths[i] = std::max(
        ui_tokens::kMinimumTouchHeight,
        font_text_width_pixels(sample, pixel_size, FontWeight::Medium) +
            2 * kCellPadding);
    total += cell_widths[i];
    if (i + 1U < count) {
      const char *separator = separator_after(kind, i);
      total += 2 * kSeparatorGap +
               (separator[0] != '\0'
                    ? font_text_width_pixels(separator, pixel_size,
                                             FontWeight::Medium)
                    : 0);
    }
  }
  int x = box.x + (box.width - total) / 2;
  for (size_t i = 0U; i < count; ++i) {
    if (out != nullptr && i < capacity)
      out[i] = {x, box.y + kCellInset, cell_widths[i],
                box.height - 2 * kCellInset};
    x += cell_widths[i];
    if (i + 1U < count) {
      const char *separator = separator_after(kind, i);
      x += 2 * kSeparatorGap +
           (separator[0] != '\0'
                ? font_text_width_pixels(separator, pixel_size,
                                         FontWeight::Medium)
                : 0);
    }
  }
  return count < capacity ? count : capacity;
}

void digit_fields_draw(Canvas &canvas, const prototype_ui::Rect &box,
                       const DigitFields &fields, uint8_t pixel_size) {
  prototype_ui::Rect cells[DigitFields::kMaxFields];
  const size_t count =
      digit_field_cells(box, fields.kind(), pixel_size, cells,
                        DigitFields::kMaxFields);
  for (size_t i = 0U; i < count; ++i) {
    const prototype_ui::Rect &cell = cells[i];
    const bool active = i == fields.active_field();
    char text[5] = {};
    std::memcpy(text, fields.digits() + fields.field_offset(i),
                fields.field_width(i));
    const int text_width =
        font_text_width_pixels(text, pixel_size, FontWeight::Medium);
    const int text_y = cell.y + (cell.height - pixel_size) / 2;
    if (active)
      canvas.fill_rect(cell.x, cell.y, cell.width, cell.height,
                       GrayLevel::Black);
    else
      canvas.fill_rect(cell.x, cell.y + cell.height - ui_tokens::kIconStroke,
                       cell.width, ui_tokens::kIconStroke, GrayLevel::Black);
    canvas.draw_text_pixels(cell.x + (cell.width - text_width) / 2, text_y,
                            text, pixel_size,
                            active ? GrayLevel::White : GrayLevel::Black,
                            FontWeight::Medium);
    if (i + 1U < count) {
      const char *separator = separator_after(fields.kind(), i);
      if (separator[0] != '\0')
        canvas.draw_text_pixels(cell.x + cell.width + kSeparatorGap, text_y,
                                separator, pixel_size, GrayLevel::Black,
                                FontWeight::Medium);
    }
  }
}
