#include <cassert>
#include <cstring>

#include "checkup_service.h"

int main()
{
    CheckupService service;
    Checkup item = {};
    item.date = {2026U, 9U, 20U};
    std::strcpy(item.title, "ROUTINE CHECKUP");
    uint32_t id = 0U;
    assert(service.create(item, &id));
    assert(id != 0U && service.count() == 1U);
    assert(service.next(1U) != nullptr);
    assert(service.complete(id));
    assert(service.next(1U) == nullptr);
    assert(service.remove(id));
    assert(service.count() == 0U);
    for (size_t index = 0U; index < CheckupService::kCapacity; ++index) {
        item.id = 0U;
        item.date.day = static_cast<uint8_t>(index + 1U);
        assert(service.create(item));
    }
    item.id = 0U;
    assert(!service.create(item));
    return 0;
}
