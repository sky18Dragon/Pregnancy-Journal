#include "reminder_service.h"

#include <cstring>

bool reminder_valid(const Reminder &reminder)
{
    return reminder.schema_version == kReminderSchemaVersion &&
           reminder.id != 0U && pregnancy_date_valid(reminder.date) &&
           reminder.hour <= 23U && reminder.minute <= 59U &&
           reminder.title[0] != '\0' &&
           reminder.type <= ReminderType::Rest &&
           reminder.repeat_rule <= RepeatRule::Weekly;
}

bool reminder_epoch(const Reminder &reminder, uint32_t &epoch_seconds)
{
    int32_t day_index = 0;
    if (!pregnancy_date_to_day_index(reminder.date, day_index) || day_index < 0 ||
        reminder.hour > 23U || reminder.minute > 59U) return false;
    epoch_seconds = static_cast<uint32_t>(day_index) * 86400U +
                    static_cast<uint32_t>(reminder.hour) * 3600U +
                    static_cast<uint32_t>(reminder.minute) * 60U;
    return true;
}

const char *reminder_type_name(ReminderType type)
{
    switch (type) {
    case ReminderType::General: return "GENERAL";
    case ReminderType::Supplement: return "SUPPLEMENT";
    case ReminderType::Checkup: return "CHECKUP";
    case ReminderType::Water: return "WATER";
    case ReminderType::Activity: return "ACTIVITY";
    case ReminderType::Rest: return "REST";
    }
    return "GENERAL";
}

bool ReminderService::create(Reminder reminder, uint32_t *created_id)
{
    if (count_ >= kCapacity) return false;
    if (reminder.id == 0U) reminder.id = next_id_++;
    if (!reminder_valid(reminder) || find(reminder.id) != nullptr) return false;
    if (reminder.id >= next_id_) next_id_ = reminder.id + 1U;
    reminders_[count_++] = reminder;
    if (created_id != nullptr) *created_id = reminder.id;
    return true;
}

bool ReminderService::update(const Reminder &reminder)
{
    Reminder *current = find(reminder.id);
    if (current == nullptr || !reminder_valid(reminder)) return false;
    *current = reminder;
    return true;
}

bool ReminderService::remove(uint32_t id)
{
    for (size_t index = 0U; index < count_; ++index) {
        if (reminders_[index].id != id) continue;
        for (size_t move = index + 1U; move < count_; ++move)
            reminders_[move - 1U] = reminders_[move];
        reminders_[--count_] = {};
        return true;
    }
    return false;
}

bool ReminderService::complete(uint32_t id)
{
    Reminder *reminder = find(id);
    if (reminder == nullptr) return false;
    if (reminder->repeat_rule == RepeatRule::None) {
        reminder->completed = true;
        return true;
    }
    int32_t day = 0;
    PregnancyDate next_date = {};
    const int increment = reminder->repeat_rule == RepeatRule::Daily ? 1 : 7;
    if (!pregnancy_date_to_day_index(reminder->date, day) ||
        !pregnancy_date_from_day_index(day + increment, next_date)) return false;
    reminder->date = next_date;
    reminder->completed = false;
    return true;
}

void ReminderService::clear()
{
    for (Reminder &reminder : reminders_) reminder = {};
    count_ = 0U;
    next_id_ = 1U;
}

const Reminder *ReminderService::at(size_t index) const
{
    return index < count_ ? &reminders_[index] : nullptr;
}

Reminder *ReminderService::find(uint32_t id)
{
    for (size_t index = 0U; index < count_; ++index)
        if (reminders_[index].id == id) return &reminders_[index];
    return nullptr;
}

const Reminder *ReminderService::find(uint32_t id) const
{
    for (size_t index = 0U; index < count_; ++index)
        if (reminders_[index].id == id) return &reminders_[index];
    return nullptr;
}

size_t ReminderService::get_today(const PregnancyDate &today,
                                  Reminder *output[], size_t capacity)
{
    size_t found = 0U;
    for (size_t index = 0U; index < count_ && found < capacity; ++index) {
        const Reminder &item = reminders_[index];
        if (item.date.year == today.year && item.date.month == today.month &&
            item.date.day == today.day) output[found++] = &reminders_[index];
    }
    return found;
}

size_t ReminderService::get_upcoming(uint32_t now, const Reminder *output[],
                                     size_t capacity) const
{
    size_t found = 0U;
    for (size_t index = 0U; index < count_ && found < capacity; ++index) {
        uint32_t epoch = 0U;
        if (!reminders_[index].completed && reminder_epoch(reminders_[index], epoch) &&
            epoch > now) output[found++] = &reminders_[index];
    }
    for (size_t left = 0U; left < found; ++left) {
        for (size_t right = left + 1U; right < found; ++right) {
            uint32_t left_epoch = 0U, right_epoch = 0U;
            reminder_epoch(*output[left], left_epoch);
            reminder_epoch(*output[right], right_epoch);
            if (right_epoch < left_epoch) {
                const Reminder *swap = output[left];
                output[left] = output[right];
                output[right] = swap;
            }
        }
    }
    return found;
}

const Reminder *ReminderService::next(uint32_t now, uint32_t *epoch_seconds) const
{
    const Reminder *best = nullptr;
    uint32_t best_epoch = 0U;
    for (size_t index = 0U; index < count_; ++index) {
        uint32_t epoch = 0U;
        if (reminders_[index].completed || !reminder_epoch(reminders_[index], epoch) ||
            epoch <= now || (best != nullptr && epoch >= best_epoch)) continue;
        best = &reminders_[index];
        best_epoch = epoch;
    }
    if (best != nullptr && epoch_seconds != nullptr) *epoch_seconds = best_epoch;
    return best;
}
