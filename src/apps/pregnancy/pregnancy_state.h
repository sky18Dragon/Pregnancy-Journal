#pragma once

#include <cstdint>

struct PregnancyDate {
    uint16_t year = 0U;
    uint8_t month = 0U;
    uint8_t day = 0U;
};

enum class PregnancyStage : uint8_t {
    FirstTrimester,
    SecondTrimester,
    ThirdTrimester,
};

struct PregnancyProgress {
    uint16_t weeks = 0U;
    uint8_t days = 0U;
    uint8_t percent = 0U;
    uint16_t gestational_days = 0U;
    PregnancyStage stage = PregnancyStage::FirstTrimester;
};

// The tracker accepts week 0 through 42+0. Progress is capped at 40 weeks.
// 孕周看板接受0周到42周，40周之后进度保持100%。
constexpr uint16_t kPregnancyNominalDays = 280U;
constexpr uint16_t kPregnancyMaximumDays = 294U;

bool pregnancy_date_valid(const PregnancyDate &date);
bool pregnancy_date_to_day_index(const PregnancyDate &date,
                                 int32_t &day_index);
bool pregnancy_date_from_day_index(int32_t day_index,
                                   PregnancyDate &date);

// Calculates gestational age from a clinician-confirmed estimated due date.
// 根据确认过的预产期计算当前孕周、阶段和40周进度。
bool pregnancy_progress_calculate(const PregnancyDate &today,
                                  const PregnancyDate &due_date,
                                  PregnancyProgress &progress);

const char *pregnancy_stage_name(PregnancyStage stage);
