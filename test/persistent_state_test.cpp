#include <cassert>

#include "persistent_state.h"

int main()
{
    assert(!sticky_app_id_valid(static_cast<StickyAppId>(0)));
    assert(sticky_app_id_valid(StickyAppId::Pregnancy));
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
    // The NVS record layout is part of the stored format: adding preference
    // fields must not change it, or old records stop validating.
    static_assert(sizeof(StickyLegacyDeviceSettings) == 8U,
                  "legacy NVS settings layout changed");
    assert(first.payload_size == sizeof(StickyLegacyDeviceSettings));
    // Newer preference fields take defaults when read from a legacy record.
    settings.shortcut_target = StickyShortcutTarget::Off;
    settings.kick_mode_pref = StickyKickModePref::Awareness;
    const StickyPersistentRecord with_new_fields = sticky_persistent_make(settings, 6U);
    assert(sticky_persistent_valid(with_new_fields));
    const StickyDeviceSettings read_back = sticky_persistent_settings(with_new_fields);
    assert(read_back.language == UiLanguage::ChineseSimplified);
    assert(read_back.last_app == StickyAppId::Settings);
    assert(read_back.shortcut_target == StickyShortcutTarget::WeightRecordToday);
    assert(read_back.kick_mode_pref == StickyKickModePref::Unset);
    return 0;
}
