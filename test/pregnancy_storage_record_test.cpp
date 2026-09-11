#include <cassert>

#include "pregnancy_storage_record.h"

int main()
{
    constexpr PregnancyDate kDueDate = {2027U, 3U, 18U};
    PregnancyStorageRecord record =
        pregnancy_storage_record_make(kDueDate);
    assert(pregnancy_storage_record_validate(record));
    const PregnancyDate restored =
        pregnancy_storage_record_due_date(record);
    assert(restored.year == kDueDate.year);
    assert(restored.month == kDueDate.month);
    assert(restored.day == kDueDate.day);

    record.due_day = 19U;
    assert(!pregnancy_storage_record_validate(record));

    record = pregnancy_storage_record_make({2027U, 2U, 29U});
    assert(!pregnancy_storage_record_validate(record));
    return 0;
}
