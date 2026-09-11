#include "persistent_state.h"

#include <cstddef>

namespace {

uint32_t fnv1a(const uint8_t *data, size_t size)
{
    uint32_t hash = 2166136261U;
    for (size_t index = 0U; index < size; ++index) {
        hash ^= data[index];
        hash *= 16777619U;
    }
    return hash;
}

}  // namespace

uint32_t sticky_persistent_checksum(const StickyPersistentRecord &record)
{
    return fnv1a(reinterpret_cast<const uint8_t *>(&record),
                 offsetof(StickyPersistentRecord, checksum));
}

StickyPersistentRecord sticky_persistent_make(
    const StickyDeviceSettings &settings, uint32_t sequence)
{
    StickyPersistentRecord record = {};
    record.magic = kStickyPersistentMagic;
    record.version = kStickyPersistentVersion;
    record.payload_size = sizeof(StickyDeviceSettings);
    record.sequence = sequence;
    record.settings = settings;
    record.checksum = sticky_persistent_checksum(record);
    return record;
}

bool sticky_persistent_valid(const StickyPersistentRecord &record)
{
    return record.magic == kStickyPersistentMagic &&
           record.version == kStickyPersistentVersion &&
           record.payload_size == sizeof(StickyDeviceSettings) &&
           record.checksum == sticky_persistent_checksum(record);
}

bool sticky_persistent_select(const StickyPersistentRecord *first,
                              const StickyPersistentRecord *second,
                              StickyPersistentRecord &selected)
{
    const bool first_valid = first != nullptr && sticky_persistent_valid(*first);
    const bool second_valid = second != nullptr && sticky_persistent_valid(*second);
    if (!first_valid && !second_valid) {
        return false;
    }
    if (!second_valid || (first_valid && first->sequence >= second->sequence)) {
        selected = *first;
    } else {
        selected = *second;
    }
    return true;
}
