#include <cassert>

#include "pregnancy_state.h"

namespace {

PregnancyDate date_for_gestational_day(const PregnancyDate &due_date,
                                       int gestational_day)
{
    int32_t due_index = 0;
    assert(pregnancy_date_to_day_index(due_date, due_index));
    PregnancyDate today = {};
    assert(pregnancy_date_from_day_index(
        due_index - static_cast<int32_t>(kPregnancyNominalDays) +
            gestational_day,
        today));
    return today;
}

PregnancyProgress progress_at(const PregnancyDate &due_date,
                              int gestational_day)
{
    PregnancyProgress progress = {};
    assert(pregnancy_progress_calculate(
        date_for_gestational_day(due_date, gestational_day),
        due_date,
        progress));
    return progress;
}

}  // namespace

int main()
{
    assert(pregnancy_date_valid({2028U, 2U, 29U}));
    assert(!pregnancy_date_valid({2027U, 2U, 29U}));
    assert(!pregnancy_date_valid({2027U, 13U, 1U}));

    int32_t leap_index = 0;
    assert(pregnancy_date_to_day_index({2028U, 2U, 29U}, leap_index));
    PregnancyDate round_trip = {};
    assert(pregnancy_date_from_day_index(leap_index, round_trip));
    assert(round_trip.year == 2028U);
    assert(round_trip.month == 2U);
    assert(round_trip.day == 29U);

    constexpr PregnancyDate kDueDate = {2027U, 1U, 1U};
    PregnancyProgress progress = progress_at(kDueDate, 97);
    assert(progress.weeks == 13U && progress.days == 6U);
    assert(progress.stage == PregnancyStage::FirstTrimester);

    progress = progress_at(kDueDate, 98);
    assert(progress.weeks == 14U && progress.days == 0U);
    assert(progress.percent == 35U);
    assert(progress.stage == PregnancyStage::SecondTrimester);

    progress = progress_at(kDueDate, 195);
    assert(progress.stage == PregnancyStage::SecondTrimester);
    progress = progress_at(kDueDate, 196);
    assert(progress.weeks == 28U && progress.days == 0U);
    assert(progress.stage == PregnancyStage::ThirdTrimester);

    progress = progress_at(kDueDate, 280);
    assert(progress.weeks == 40U && progress.days == 0U);
    assert(progress.percent == 100U);
    progress = progress_at(kDueDate, 294);
    assert(progress.weeks == 42U && progress.days == 0U);
    assert(progress.percent == 100U);

    PregnancyProgress invalid = {};
    assert(!pregnancy_progress_calculate(
        date_for_gestational_day(kDueDate, -1), kDueDate, invalid));
    assert(!pregnancy_progress_calculate(
        date_for_gestational_day(kDueDate, 295), kDueDate, invalid));

    assert(pregnancy_stage_name(PregnancyStage::FirstTrimester) != nullptr);
    assert(pregnancy_stage_name(PregnancyStage::SecondTrimester) != nullptr);
    assert(pregnancy_stage_name(PregnancyStage::ThirdTrimester) != nullptr);
    return 0;
}
