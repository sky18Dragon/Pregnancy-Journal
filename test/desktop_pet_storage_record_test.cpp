#include <cassert>
#include <cstdint>
#include <cstring>

#include "desktop_pet_storage_record.h"

int main()
{
    DesktopPetState first = {};
    first.pet.stage = PetLifeStage::Child;
    first.pet.growth = 74U;
    std::memcpy(first.name, "MOMO", 5U);
    first.outing_plan.decision_day_key = 20000U;
    first.outing_plan.departure_epoch_seconds = 1728032400U;
    first.outing_plan.return_epoch_seconds = 1728039600U;

    DesktopPetState second = first;
    second.pet.growth = 80U;
    std::memcpy(second.name, "LULU", 5U);

    DesktopPetStorageRecord records[kPetSaveSlotCount] = {
        desktop_pet_storage_record_make(first, 10U, 1728030000U),
        desktop_pet_storage_record_make(second, 11U, 1728030300U),
    };
    assert(desktop_pet_storage_record_validate(records[0]));
    assert(desktop_pet_storage_record_validate(records[1]));
    assert(desktop_pet_storage_record_select_newest(
               records, kPetSaveSlotCount) == 1);
    assert(desktop_pet_storage_record_select_write_slot(
               records, kPetSaveSlotCount) == 0U);

    records[1].state.name[0] = 'X';
    assert(!desktop_pet_storage_record_validate(records[1]));
    assert(desktop_pet_storage_record_select_newest(
               records, kPetSaveSlotCount) == 0);
    assert(desktop_pet_storage_record_select_write_slot(
               records, kPetSaveSlotCount) == 1U);

    records[0] = desktop_pet_storage_record_make(
        first, UINT32_MAX, 1728030000U);
    records[1] = desktop_pet_storage_record_make(
        second, 0U, 1728030300U);
    assert(desktop_pet_storage_record_select_newest(
               records, kPetSaveSlotCount) == 1);
    return 0;
}
