#include "app_registry.h"

#include "book_of_answers_app.h"
#include "desktop_pet_app.h"
#include "home_app.h"
#include "pomodoro_app.h"
#include "pregnancy_app.h"
#include "settings_app.h"
#include "status_board_app.h"

namespace {

bool allow_sleep() { return true; }
uint32_t pet_timeout() { return 10U * 60U * 1000U; }
esp_err_t simple_book_sleep(uint32_t &now, uint32_t &next)
{ now = 0U; next = 0U; return book_of_answers_app_prepare_power_sleep(); }
esp_err_t simple_pomodoro_sleep(uint32_t &now, uint32_t &next)
{ now = 0U; next = 0U; return pomodoro_app_prepare_power_sleep(); }
esp_err_t simple_status_sleep(uint32_t &now, uint32_t &next)
{ now = 0U; next = 0U; return status_board_app_prepare_power_sleep(); }

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
    {StickyAppId::DesktopPet, "desktop_pet", "Pet",
     StickyAppRotationPolicy::FixedPortrait,
     desktop_pet_app_start, desktop_pet_app_pause, desktop_pet_app_resume,
     desktop_pet_app_prepare_power_sleep, pet_timeout,
     desktop_pet_app_power_sleep_allowed},
    {StickyAppId::Pomodoro, "pomodoro", "Pomodoro",
     StickyAppRotationPolicy::FollowDevice,
     pomodoro_app_start, pomodoro_app_pause, pomodoro_app_resume,
     simple_pomodoro_sleep, pomodoro_app_power_sleep_timeout_ms,
     pomodoro_app_power_sleep_allowed},
    {StickyAppId::StatusBoard, "status_board", "Status",
     StickyAppRotationPolicy::FollowDevice,
     status_board_app_start, status_board_app_pause, status_board_app_resume,
     simple_status_sleep, status_board_app_power_sleep_timeout_ms, allow_sleep},
    {StickyAppId::BookOfAnswers, "book_of_answers", "Answers",
     StickyAppRotationPolicy::FixedPortrait,
     book_of_answers_app_start, book_of_answers_app_pause,
     book_of_answers_app_resume, simple_book_sleep,
     book_of_answers_app_power_sleep_timeout_ms,
     book_of_answers_app_power_sleep_allowed},
    {StickyAppId::Pregnancy, "pregnancy", "Baby Week",
     StickyAppRotationPolicy::FixedLandscape,
     pregnancy_app_start, pregnancy_app_pause, pregnancy_app_resume,
     pregnancy_app_prepare_power_sleep, pregnancy_app_power_sleep_timeout_ms,
     allow_sleep},
};

}  // namespace

const StickyAppDescriptor *sticky_app_registry_data() { return kApps; }
size_t sticky_app_registry_count() { return sizeof(kApps) / sizeof(kApps[0]); }

const StickyAppDescriptor *sticky_app_registry_find(StickyAppId id)
{
    for (const StickyAppDescriptor &app : kApps) {
        if (app.id == id) return &app;
    }
    return nullptr;
}
