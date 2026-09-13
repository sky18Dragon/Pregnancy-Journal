#include "checkup_service.h"

bool checkup_valid(const Checkup &item)
{
    return item.schema_version == kCheckupSchemaVersion && item.id != 0U &&
           pregnancy_date_valid(item.date) && item.hour <= 23U &&
           item.minute <= 59U && item.title[0] != '\0';
}

bool checkup_epoch(const Checkup &item, uint32_t &epoch)
{
    int32_t day = 0;
    if (!checkup_valid(item) || !pregnancy_date_to_day_index(item.date, day))
        return false;
    epoch = static_cast<uint32_t>(day) * 86400U +
            static_cast<uint32_t>(item.hour) * 3600U +
            static_cast<uint32_t>(item.minute) * 60U;
    return true;
}

bool CheckupService::create(Checkup item, uint32_t *created_id)
{
    if (count_ >= kCapacity) return false;
    if (item.id == 0U) item.id = next_id_++;
    if (!checkup_valid(item) || find(item.id) != nullptr) return false;
    if (item.id >= next_id_) next_id_ = item.id + 1U;
    items_[count_++] = item;
    if (created_id != nullptr) *created_id = item.id;
    return true;
}

bool CheckupService::remove(uint32_t id)
{
    for (size_t index = 0U; index < count_; ++index) {
        if (items_[index].id != id) continue;
        for (size_t move = index + 1U; move < count_; ++move)
            items_[move - 1U] = items_[move];
        items_[--count_] = {};
        return true;
    }
    return false;
}

bool CheckupService::complete(uint32_t id)
{
    Checkup *item = find(id);
    if (item == nullptr) return false;
    item->completed = true;
    return true;
}

void CheckupService::clear()
{
    for (Checkup &item : items_) item = {};
    count_ = 0U;
    next_id_ = 1U;
}

const Checkup *CheckupService::at(size_t index) const
{ return index < count_ ? &items_[index] : nullptr; }

Checkup *CheckupService::find(uint32_t id)
{
    for (size_t i = 0U; i < count_; ++i) if (items_[i].id == id) return &items_[i];
    return nullptr;
}

const Checkup *CheckupService::next(uint32_t now, uint32_t *epoch) const
{
    const Checkup *best = nullptr;
    uint32_t best_epoch = 0U;
    for (size_t i = 0U; i < count_; ++i) {
        uint32_t candidate = 0U;
        if (items_[i].completed || !checkup_epoch(items_[i], candidate) ||
            candidate <= now || (best != nullptr && candidate >= best_epoch)) continue;
        best = &items_[i];
        best_epoch = candidate;
    }
    if (best != nullptr && epoch != nullptr) *epoch = best_epoch;
    return best;
}
