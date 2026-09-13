#pragma once

#include <cstdint>

struct WeekContent {
    uint8_t week = 0U;
    const char *baby_size_text = nullptr;
    const char *baby_weight_text = nullptr;
    const char *baby_development = nullptr;
    const char *mother_changes = nullptr;
    const char *daily_advice = nullptr;
    const char *baby_development_zh = nullptr;
    const char *mother_changes_zh = nullptr;
    const char *daily_advice_zh = nullptr;
};

// Returns reviewed, offline content for every displayed week. Inputs outside
// 1..40 are safely clamped while preserving non-null text fields.
WeekContent pregnancy_content_for_week(int week);
