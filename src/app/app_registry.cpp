#include "app_registry.h"

#include "checkup_app.h"
#include "pregnancy_app.h"
#include "reminder_app.h"
#include "settings_app.h"

namespace {

bool allow_sleep() { return true; }

const StickyAppDescriptor kApps[] = {
    {StickyAppId::Pregnancy, "pregnancy", "Baby Week",
     StickyAppRotationPolicy::FixedLandscape,
     pregnancy_app_start, pregnancy_app_pause, pregnancy_app_resume,
     pregnancy_app_prepare_power_sleep, pregnancy_app_power_sleep_timeout_ms,
     allow_sleep},
    {StickyAppId::Reminder, "reminder", "Reminders",
     StickyAppRotationPolicy::FixedLandscape,
     reminder_app_start, reminder_app_pause, reminder_app_resume,
     reminder_app_prepare_power_sleep, reminder_app_power_sleep_timeout_ms,
     allow_sleep},
    {StickyAppId::Checkup, "checkup", "Checkups",
     StickyAppRotationPolicy::FixedLandscape,
     checkup_app_start, checkup_app_pause, checkup_app_resume,
     checkup_app_prepare_power_sleep, checkup_app_power_sleep_timeout_ms,
     allow_sleep},
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
