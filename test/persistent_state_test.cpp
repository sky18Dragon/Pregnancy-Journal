#include <cassert>

#include "persistent_state.h"

int main()
{
    StickyDeviceSettings settings = {};
    settings.language = UiLanguage::ChineseSimplified;
    settings.last_app = StickyAppId::Settings;
    StickyPersistentRecord first = sticky_persistent_make(settings, 4U);
    StickyPersistentRecord second = sticky_persistent_make(settings, 5U);
    assert(sticky_persistent_valid(first));
    StickyPersistentRecord selected = {};
    assert(sticky_persistent_select(&first, &second, selected));
    assert(selected.sequence == 5U);
    second.checksum ^= 1U;
    assert(sticky_persistent_select(&first, &second, selected));
    assert(selected.sequence == 4U);
    first.version = 99U;
    assert(!sticky_persistent_select(&first, &second, selected));
    return 0;
}
