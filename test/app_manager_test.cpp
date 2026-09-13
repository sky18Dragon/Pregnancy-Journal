#include <cassert>

#include "app_manager.h"

class Canvas {};

namespace {
int starts[2] = {};
int pauses[2] = {};
int resumes[2] = {};
esp_err_t start0(Canvas &) { ++starts[0]; return ESP_OK; }
esp_err_t start1(Canvas &) { ++starts[1]; return ESP_OK; }
esp_err_t pause0() { ++pauses[0]; return ESP_OK; }
esp_err_t pause1() { ++pauses[1]; return ESP_OK; }
esp_err_t resume0() { ++resumes[0]; return ESP_OK; }
esp_err_t resume1() { ++resumes[1]; return ESP_OK; }
esp_err_t sleep0(uint32_t &, uint32_t &) { return ESP_OK; }
uint32_t timeout0() { return 1U; }
bool allowed0() { return true; }
}

int main()
{
    const StickyAppDescriptor apps[] = {
        {StickyAppId::Pregnancy, "pregnancy", "Baby Week", StickyAppRotationPolicy::FixedLandscape,
         start0, pause0, resume0, sleep0, timeout0, allowed0},
        {StickyAppId::Settings, "settings", "Settings", StickyAppRotationPolicy::FixedLandscape,
         start1, pause1, resume1, sleep0, timeout0, allowed0},
    };
    Canvas canvas;
    StickyAppManager manager(apps, 2U, StickyAppId::Pregnancy);
    assert(manager.start(canvas, static_cast<StickyAppId>(77)) == ESP_OK);
    assert(manager.current_id() == StickyAppId::Pregnancy);
    assert(starts[0] == 1);
    assert(manager.switch_to(StickyAppId::Settings) == ESP_OK);
    assert(pauses[0] == 1 && starts[1] == 1);
    assert(manager.return_home() == ESP_OK);
    assert(pauses[1] == 1 && resumes[0] == 1);
    assert(manager.started(StickyAppId::Pregnancy));
    assert(manager.started(StickyAppId::Settings));
    return 0;
}
