#pragma once

#include <cstddef>
#include <cstdint>

#include "reminder.h"

class ReminderService {
public:
    static constexpr size_t kCapacity = 12U;

    bool create(Reminder reminder, uint32_t *created_id = nullptr);
    bool update(const Reminder &reminder);
    bool remove(uint32_t id);
    bool complete(uint32_t id);
    void clear();

    size_t count() const { return count_; }
    const Reminder *at(size_t index) const;
    Reminder *find(uint32_t id);
    const Reminder *find(uint32_t id) const;
    size_t get_today(const PregnancyDate &today,
                     Reminder *output[], size_t capacity);
    size_t get_upcoming(uint32_t now, const Reminder *output[],
                        size_t capacity) const;
    const Reminder *next(uint32_t now, uint32_t *epoch_seconds = nullptr) const;

private:
    Reminder reminders_[kCapacity] = {};
    size_t count_ = 0U;
    uint32_t next_id_ = 1U;
};
