#include "pregnancy_state.h"

#include <algorithm>

namespace {

constexpr uint8_t kDaysInMonth[] = {
    31U, 28U, 31U, 30U, 31U, 30U,
    31U, 31U, 30U, 31U, 30U, 31U,
};

bool leap_year(uint16_t year)
{
    return (year % 4U == 0U && year % 100U != 0U) ||
           year % 400U == 0U;
}

uint16_t days_in_year(uint16_t year)
{
    return leap_year(year) ? 366U : 365U;
}

uint8_t days_in_month(uint16_t year, uint8_t month)
{
    if (month < 1U || month > 12U) {
        return 0U;
    }
    return month == 2U && leap_year(year)
               ? 29U
               : kDaysInMonth[month - 1U];
}

}  // namespace

bool pregnancy_date_valid(const PregnancyDate &date)
{
    return date.year >= 1970U && date.year <= 2099U &&
           date.month >= 1U && date.month <= 12U &&
           date.day >= 1U &&
           date.day <= days_in_month(date.year, date.month);
}

bool pregnancy_date_to_day_index(const PregnancyDate &date,
                                 int32_t &day_index)
{
    if (!pregnancy_date_valid(date)) {
        return false;
    }

    int32_t days = 0;
    for (uint16_t year = 1970U; year < date.year; ++year) {
        days += days_in_year(year);
    }
    for (uint8_t month = 1U; month < date.month; ++month) {
        days += days_in_month(date.year, month);
    }
    days += date.day - 1U;
    day_index = days;
    return true;
}

bool pregnancy_date_from_day_index(int32_t day_index,
                                   PregnancyDate &date)
{
    if (day_index < 0) {
        return false;
    }

    uint16_t year = 1970U;
    while (year <= 2099U && day_index >= days_in_year(year)) {
        day_index -= days_in_year(year);
        ++year;
    }
    if (year > 2099U) {
        return false;
    }

    uint8_t month = 1U;
    while (month <= 12U && day_index >= days_in_month(year, month)) {
        day_index -= days_in_month(year, month);
        ++month;
    }
    if (month > 12U) {
        return false;
    }

    date.year = year;
    date.month = month;
    date.day = static_cast<uint8_t>(day_index + 1);
    return true;
}

bool pregnancy_progress_calculate(const PregnancyDate &today,
                                  const PregnancyDate &due_date,
                                  PregnancyProgress &progress)
{
    int32_t today_index = 0;
    int32_t due_index = 0;
    if (!pregnancy_date_to_day_index(today, today_index) ||
        !pregnancy_date_to_day_index(due_date, due_index)) {
        return false;
    }

    const int32_t gestational_days =
        static_cast<int32_t>(kPregnancyNominalDays) -
        (due_index - today_index);
    if (gestational_days < 0 ||
        gestational_days > static_cast<int32_t>(kPregnancyMaximumDays)) {
        return false;
    }

    progress.gestational_days =
        static_cast<uint16_t>(gestational_days);
    progress.weeks = static_cast<uint16_t>(gestational_days / 7);
    progress.days = static_cast<uint8_t>(gestational_days % 7);
    const uint32_t capped_days = std::min<uint32_t>(
        progress.gestational_days, kPregnancyNominalDays);
    progress.percent = static_cast<uint8_t>(
        (capped_days * 100U + kPregnancyNominalDays / 2U) /
        kPregnancyNominalDays);

    if (gestational_days < 14 * 7) {
        progress.stage = PregnancyStage::FirstTrimester;
    } else if (gestational_days < 28 * 7) {
        progress.stage = PregnancyStage::SecondTrimester;
    } else {
        progress.stage = PregnancyStage::ThirdTrimester;
    }
    return true;
}

const char *pregnancy_stage_name(PregnancyStage stage)
{
    switch (stage) {
    case PregnancyStage::FirstTrimester:
        return "FIRST TRIMESTER";
    case PregnancyStage::SecondTrimester:
        return "SECOND TRIMESTER";
    case PregnancyStage::ThirdTrimester:
        return "THIRD TRIMESTER";
    }
    return "UNKNOWN";
}
