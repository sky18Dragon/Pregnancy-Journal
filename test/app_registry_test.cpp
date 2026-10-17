#include <cassert>
#include <cstddef>
#include <cstdint>

#include "app_registry.h"

class Canvas;

#define DEFINE_APP_STUBS(name)                                                 \
  esp_err_t name##_app_start(Canvas &) { return ESP_OK; }                      \
  esp_err_t name##_app_pause() { return ESP_OK; }                              \
  esp_err_t name##_app_resume() { return ESP_OK; }                             \
  esp_err_t name##_app_prepare_power_sleep(uint32_t &, uint32_t &) {           \
    return ESP_OK;                                                             \
  }                                                                            \
  uint32_t name##_app_power_sleep_timeout_ms() { return 60000U; }

DEFINE_APP_STUBS(pregnancy)
DEFINE_APP_STUBS(weight)
DEFINE_APP_STUBS(kick)
DEFINE_APP_STUBS(settings)

bool settings_app_power_sleep_allowed() { return true; }
void kick_app_capture_press(int64_t) {}
void pregnancy_app_before_sleep_frame(Canvas &) {}

int main() {
  assert(sticky_app_registry_count() == 4U);
  const StickyAppId expected[] = {StickyAppId::Pregnancy, StickyAppId::Weight,
                                  StickyAppId::Kicks, StickyAppId::Settings};
  assert(static_cast<int>(StickyAppId::Settings) == 1);
  assert(static_cast<int>(StickyAppId::Pregnancy) == 2);
  assert(static_cast<int>(StickyAppId::Reminder) == 3);
  assert(static_cast<int>(StickyAppId::Checkup) == 4);
  assert(static_cast<int>(StickyAppId::Weight) == 7);
  assert(static_cast<int>(StickyAppId::Kicks) == 8);
  // Only the pregnancy dashboard draws its own battery indicator, so only it needs the hook.
  assert(sticky_app_registry_find(StickyAppId::Pregnancy)->before_sleep_frame != nullptr);
  assert(sticky_app_registry_find(StickyAppId::Weight)->before_sleep_frame == nullptr);
  assert(sticky_app_registry_find(StickyAppId::Kicks)->before_sleep_frame == nullptr);
  assert(sticky_app_registry_find(StickyAppId::Settings)->before_sleep_frame == nullptr);
  assert(sticky_app_registry_find(StickyAppId::Reminder) == nullptr);
  assert(sticky_app_registry_find(StickyAppId::Checkup) == nullptr);
  const StickyAppDescriptor *apps = sticky_app_registry_data();
  assert(apps != nullptr);
  for (size_t index = 0U; index < sticky_app_registry_count(); ++index) {
    assert(apps[index].id == expected[index]);
    assert(apps[index].rotation == StickyAppRotationPolicy::FollowDevice);
    // Only the kick counter takes the top key as an action.
    assert((apps[index].capture_press != nullptr) ==
           (apps[index].id == StickyAppId::Kicks));
  }
  return 0;
}
