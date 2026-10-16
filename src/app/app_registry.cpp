#include "app_registry.h"

#include "kick_app.h"
#include "pregnancy_app.h"
#include "settings_app.h"
#include "weight_app.h"

namespace {

bool allow_sleep() { return true; }

const StickyAppDescriptor kApps[] = {
    {StickyAppId::Pregnancy, "pregnancy", "Baby Week",
     StickyAppRotationPolicy::FollowDevice, pregnancy_app_start,
     pregnancy_app_pause, pregnancy_app_resume,
     pregnancy_app_prepare_power_sleep, pregnancy_app_power_sleep_timeout_ms,
     allow_sleep},
    {StickyAppId::Weight, "weight", "Weight",
     StickyAppRotationPolicy::FollowDevice, weight_app_start, weight_app_pause,
     weight_app_resume, weight_app_prepare_power_sleep,
     weight_app_power_sleep_timeout_ms, allow_sleep},
    {StickyAppId::Kicks, "kicks", "Kicks",
     StickyAppRotationPolicy::FollowDevice, kick_app_start, kick_app_pause,
     kick_app_resume, kick_app_prepare_power_sleep,
     kick_app_power_sleep_timeout_ms, allow_sleep, kick_app_capture_press},
    {StickyAppId::Settings, "settings", "Settings",
     StickyAppRotationPolicy::FollowDevice, settings_app_start,
     settings_app_pause, settings_app_resume, settings_app_prepare_power_sleep,
     settings_app_power_sleep_timeout_ms, settings_app_power_sleep_allowed},
};

} // namespace

const StickyAppDescriptor *sticky_app_registry_data() { return kApps; }
size_t sticky_app_registry_count() { return sizeof(kApps) / sizeof(kApps[0]); }

const StickyAppDescriptor *sticky_app_registry_find(StickyAppId id) {
  for (const StickyAppDescriptor &app : kApps) {
    if (app.id == id)
      return &app;
  }
  return nullptr;
}
