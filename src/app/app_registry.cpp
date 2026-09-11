#include "app_registry.h"

#include "home_app.h"
#include "settings_app.h"

namespace {

bool allow_sleep() { return true; }

const StickyAppDescriptor kApps[] = {
    {StickyAppId::Home, "home", "Home",
     StickyAppRotationPolicy::FixedLandscape,
     home_app_start, home_app_pause, home_app_resume,
     home_app_prepare_power_sleep, home_app_power_sleep_timeout_ms, allow_sleep},
    {StickyAppId::Settings, "settings", "Settings",
     StickyAppRotationPolicy::FixedLandscape,
     settings_app_start, settings_app_pause, settings_app_resume,
     settings_app_prepare_power_sleep, settings_app_power_sleep_timeout_ms,
     settings_app_power_sleep_allowed},
};

}  // namespace

const StickyAppDescriptor *sticky_app_registry_data() { return kApps; }
size_t sticky_app_registry_count()
{
    return sizeof(kApps) / sizeof(kApps[0]);
}

const StickyAppDescriptor *sticky_app_registry_find(StickyAppId id)
{
    for (const StickyAppDescriptor &app : kApps) {
        if (app.id == id) return &app;
    }
    return nullptr;
}
