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
DEFINE_APP_STUBS(reminder)
DEFINE_APP_STUBS(checkup)
DEFINE_APP_STUBS(weight)
DEFINE_APP_STUBS(kick)
DEFINE_APP_STUBS(settings)

bool settings_app_power_sleep_allowed() { return true; }

int main() {
  assert(sticky_app_registry_count() == 6U);
  const StickyAppDescriptor *apps = sticky_app_registry_data();
  assert(apps != nullptr);
  for (size_t index = 0U; index < sticky_app_registry_count(); ++index)
    assert(apps[index].rotation == StickyAppRotationPolicy::FollowDevice);
  return 0;
}
