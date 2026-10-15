#pragma once

#include <cstddef>
#include <cstdint>

#include "canvas.h"
#include "prototype_ui.h"

// Segmented numeric input for dates and times.
//
// The value is shown as separate fields (year / month / day / hour / minute).
// Tapping a field selects it, and the next digits replace only that field, so
// changing a single part no longer means retyping the whole value. Impossible
// digits (month "13", hour "25"...) are rejected as they are typed, and a
// completed field moves on to the next. Full-date validity (days per month,
// leap years) is still checked by the caller on save.
// 分段数字输入：点选某一段只改该段；输入时即时拒绝不可能的数字；填满自动跳到下一段。
enum class DigitFieldKind : uint8_t {
  DateTime, // YYYY MM DD HH MM
  Date,     // YYYY MM DD
};

class DigitFields {
public:
  static constexpr size_t kMaxDigits = 12U;
  static constexpr size_t kMaxFields = 5U;

  explicit DigitFields(DigitFieldKind kind = DigitFieldKind::DateTime) {
    begin(kind);
  }

  // Clears every digit and selects the first field.
  void begin(DigitFieldKind kind);
  // Loads exactly total() ASCII digits (a fully known value) and selects the
  // first field. Returns false, leaving the fields empty, otherwise.
  bool set_digits(const char *digits);

  DigitFieldKind kind() const { return kind_; }
  size_t field_count() const;
  size_t total() const;
  size_t field_offset(size_t index) const;
  size_t field_width(size_t index) const;
  size_t active_field() const { return active_; }

  // Selects `index`; the next digit replaces that field's current content.
  void select_field(size_t index);
  // Writes a digit into the selected field. Returns false (and changes
  // nothing) when the digit cannot occur at that position.
  bool input_digit(char digit);
  // Clears the typed digits of the selected field; with nothing typed it
  // selects the previous field instead.
  void erase();

  bool field_complete(size_t index) const;
  bool complete() const;
  // total() characters; positions not yet typed read '_'.
  const char *digits() const { return digits_; }

private:
  bool digit_allowed(size_t field, size_t position, char digit) const;
  void clear_field(size_t index);

  DigitFieldKind kind_ = DigitFieldKind::DateTime;
  char digits_[kMaxDigits + 1U] = {};
  size_t active_ = 0U;
  size_t cursor_ = 0U; // digits typed into the active field since selection
  bool armed_ = true;  // the next digit replaces the whole active field
};

// Geometry of the fields inside `box`, independent of the digits typed: the
// same rectangles are drawn by digit_fields_draw() and used as tap targets.
// Writes at most `capacity` cells and returns the field count.
size_t digit_field_cells(const prototype_ui::Rect &box, DigitFieldKind kind,
                         uint8_t pixel_size, prototype_ui::Rect *out,
                         size_t capacity);

// Draws the separators, the underlined fields and the selected field inverted.
void digit_fields_draw(Canvas &canvas, const prototype_ui::Rect &box,
                       const DigitFields &fields, uint8_t pixel_size);
