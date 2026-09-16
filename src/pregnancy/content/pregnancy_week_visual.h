#pragma once

#include <cstdint>

struct PregnancyWeekVisual {
  uint8_t week;
  const char *object_en;
  const char *object_zh;
  const char *sentence_en;
  const char *sentence_zh;
};

// Weeks 6-40 each use a different familiar object and sentence.
const PregnancyWeekVisual &pregnancy_week_visual(uint8_t week);
