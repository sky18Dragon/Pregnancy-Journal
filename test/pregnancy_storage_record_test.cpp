#include <cassert>

#include "pregnancy_storage_record.h"

int main()
{
    constexpr PregnancyDate kDueDate = {2027U, 3U, 18U};
    PregnancyProfile profile = {};
    assert(pregnancy_profile_from_due_date(kDueDate, profile));
    PregnancyStorageRecord record = pregnancy_storage_record_make(profile);
    assert(pregnancy_storage_record_validate(record));
    const PregnancyDate restored =
        pregnancy_storage_record_due_date(record);
    assert(restored.year == kDueDate.year);
    assert(restored.month == kDueDate.month);
    assert(restored.day == kDueDate.day);
    const PregnancyProfile restored_profile =
        pregnancy_storage_record_profile(record);
    assert(restored_profile.use_due_date_as_primary);
    assert(pregnancy_profile_valid(restored_profile));

    record.due_day = 19U;
    assert(!pregnancy_storage_record_validate(record));

    record = pregnancy_storage_record_make(
        PregnancyDate{2027U, 2U, 29U});
    assert(!pregnancy_storage_record_validate(record));

    assert(pregnancy_profile_from_lmp({2026U, 6U, 11U}, profile));
    record = pregnancy_storage_record_make(profile);
    assert(pregnancy_storage_record_validate(record));
    assert(!pregnancy_storage_record_profile(record).use_due_date_as_primary);
    return 0;
}
