#pragma once

#include <cstdint>

struct PregnancyDate {
    uint16_t year = 0U;
    uint8_t month = 0U;
    uint8_t day = 0U;
};

constexpr uint8_t kPregnancyProfileSchemaVersion = 1U;

struct PregnancyProfile {
    uint8_t schema_version = kPregnancyProfileSchemaVersion;
    PregnancyDate last_menstrual_period = {};
    PregnancyDate estimated_due_date = {};
    bool use_due_date_as_primary = true;
};

enum class PregnancyStage : uint8_t {
    FirstTrimester,
    SecondTrimester,
    ThirdTrimester,
};

struct PregnancyProgress {
    int16_t weeks = 0;
    uint8_t days = 0U;
    uint8_t percent = 0U;
    int16_t gestational_days = 0;
    int16_t days_until_due_date = 0;
    float progress = 0.0F;
    PregnancyStage stage = PregnancyStage::FirstTrimester;
    bool before_start = false;
    bool overdue = false;
};

// Dates before and after the nominal term remain representable. The progress
// bar alone is capped at the 40-week nominal duration.
// 预产期前后的日期都可表示，仅进度条在标准40周处封顶。
constexpr uint16_t kPregnancyNominalDays = 280U;

bool pregnancy_date_valid(const PregnancyDate &date);
bool pregnancy_date_to_day_index(const PregnancyDate &date,
                                 int32_t &day_index);
bool pregnancy_date_from_day_index(int32_t day_index,
                                   PregnancyDate &date);

bool pregnancy_profile_from_due_date(const PregnancyDate &due_date,
                                     PregnancyProfile &profile);
bool pregnancy_profile_from_lmp(const PregnancyDate &last_menstrual_period,
                                PregnancyProfile &profile);
bool pregnancy_profile_valid(const PregnancyProfile &profile);

bool pregnancy_progress_calculate(const PregnancyDate &today,
                                  const PregnancyProfile &profile,
                                  PregnancyProgress &progress);

// Calculates gestational age from a clinician-confirmed estimated due date.
// 根据确认过的预产期计算当前孕周、阶段和40周进度。
bool pregnancy_progress_calculate(const PregnancyDate &today,
                                  const PregnancyDate &due_date,
                                  PregnancyProgress &progress);

const char *pregnancy_stage_name(PregnancyStage stage);
