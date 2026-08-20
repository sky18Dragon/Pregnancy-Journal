#include "desktop_pet_storage_record.h"

#include <type_traits>

static_assert(std::is_trivially_copyable<DesktopPetState>::value,
              "DesktopPetState must remain byte-copyable for NVS storage");

DesktopPetStorageRecord desktop_pet_storage_record_make(
    const DesktopPetState &state,
    uint32_t sequence,
    uint32_t saved_epoch_seconds)
{
    DesktopPetStorageRecord record = {};
    record.state = state;
    record.header = pet_save_make_header(
        &record.state,
        sizeof(record.state),
        kDesktopPetStorageFormatVersion,
        sequence,
        saved_epoch_seconds);
    return record;
}

bool desktop_pet_storage_record_validate(
    const DesktopPetStorageRecord &record)
{
    return record.state.version == kDesktopPetStateVersion &&
           pet_save_validate_payload(
               record.header,
               &record.state,
               sizeof(record.state),
               kDesktopPetStorageFormatVersion);
}

int desktop_pet_storage_record_select_newest(
    const DesktopPetStorageRecord *records,
    size_t count)
{
    if (records == nullptr || count == 0U) {
        return -1;
    }
    PetSaveHeader headers[kPetSaveSlotCount] = {};
    bool valid[kPetSaveSlotCount] = {};
    const size_t bounded_count = count > kPetSaveSlotCount
                                     ? kPetSaveSlotCount
                                     : count;
    for (size_t index = 0U; index < bounded_count; ++index) {
        headers[index] = records[index].header;
        valid[index] = desktop_pet_storage_record_validate(
            records[index]);
    }
    return pet_save_select_newest_header(
        headers, valid, bounded_count);
}

size_t desktop_pet_storage_record_select_write_slot(
    const DesktopPetStorageRecord *records,
    size_t count)
{
    if (records == nullptr || count == 0U) {
        return 0U;
    }
    PetSaveHeader headers[kPetSaveSlotCount] = {};
    bool valid[kPetSaveSlotCount] = {};
    const size_t bounded_count = count > kPetSaveSlotCount
                                     ? kPetSaveSlotCount
                                     : count;
    for (size_t index = 0U; index < bounded_count; ++index) {
        headers[index] = records[index].header;
        valid[index] = desktop_pet_storage_record_validate(
            records[index]);
    }
    return pet_save_select_write_header(
        headers, valid, bounded_count);
}
