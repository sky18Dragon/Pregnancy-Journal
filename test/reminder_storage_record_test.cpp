#include <cassert>
#include <cstring>

#include "reminder_storage_record.h"

int main()
{
    Reminder reminder = {};
    reminder.id = 7U;
    reminder.date = {2026U, 9U, 12U};
    reminder.hour = 9U;
    reminder.minute = 30U;
    std::strcpy(reminder.title, "SUPPLEMENT");
    ReminderStorageRecord record = reminder_storage_record_make(reminder);
    assert(reminder_storage_record_valid(record));
    record.reminder.minute = 31U;
    assert(!reminder_storage_record_valid(record));
    return 0;
}
