#include "pet_save_record.h"

#include <cstring>

uint16_t pet_save_checksum(const uint8_t *data, size_t size)
{
    if (data == nullptr) {
        return 0U;
    }
    uint16_t sum = 0xFFFFU;
    for (size_t index = 0U; index < size; ++index) {
        sum ^= data[index];
        sum = static_cast<uint16_t>((sum << 3U) | (sum >> 13U));
        sum = static_cast<uint16_t>(sum + data[index]);
    }
    return sum;
}

PetSaveRecord pet_save_make_record(const PetCoreState &state,
                                   uint32_t sequence,
                                   uint32_t saved_epoch_seconds)
{
    PetSaveRecord record = {};
    record.header.magic = kPetSaveMagic;
    record.header.format_version = kPetSaveFormatVersion;
    record.header.state_size = sizeof(PetCoreState);
    record.header.sequence = sequence;
    record.header.saved_epoch_seconds = saved_epoch_seconds;
    record.state = state;
    record.header.checksum = pet_save_checksum(
        reinterpret_cast<const uint8_t *>(&record.state),
        sizeof(record.state));
    return record;
}

bool pet_save_validate(const PetSaveRecord &record)
{
    if (record.header.magic != kPetSaveMagic ||
        record.header.format_version != kPetSaveFormatVersion ||
        record.header.state_size != sizeof(PetCoreState) ||
        record.state.version != kPetCoreStateVersion) {
        return false;
    }
    const uint16_t checksum = pet_save_checksum(
        reinterpret_cast<const uint8_t *>(&record.state),
        sizeof(record.state));
    return checksum == record.header.checksum;
}

int pet_save_select_newest(const PetSaveRecord *records, size_t count)
{
    if (records == nullptr || count == 0U) {
        return -1;
    }
    int newest = -1;
    uint32_t newest_sequence = 0U;
    for (size_t index = 0U; index < count; ++index) {
        if (!pet_save_validate(records[index])) {
            continue;
        }
        if (newest < 0 || records[index].header.sequence > newest_sequence) {
            newest = static_cast<int>(index);
            newest_sequence = records[index].header.sequence;
        }
    }
    return newest;
}

size_t pet_save_select_write_slot(const PetSaveRecord *records, size_t count)
{
    if (records == nullptr || count == 0U) {
        return 0U;
    }
    for (size_t index = 0U; index < count; ++index) {
        if (!pet_save_validate(records[index])) {
            return index;
        }
    }
    size_t oldest = 0U;
    for (size_t index = 1U; index < count; ++index) {
        if (records[index].header.sequence <
            records[oldest].header.sequence) {
            oldest = index;
        }
    }
    return oldest;
}
