#include "app_log.h"

namespace {

esp_log_level_t application_log_level()
{
#if STICKY_LOG_COMPILE_LEVEL >= STICKY_LOG_LEVEL_TRACE
    return ESP_LOG_VERBOSE;
#elif STICKY_LOG_COMPILE_LEVEL >= STICKY_LOG_LEVEL_DEBUG
    return ESP_LOG_DEBUG;
#elif STICKY_LOG_COMPILE_LEVEL >= STICKY_LOG_LEVEL_INFO
    return ESP_LOG_INFO;
#elif STICKY_LOG_COMPILE_LEVEL >= STICKY_LOG_LEVEL_WARN
    return ESP_LOG_WARN;
#elif STICKY_LOG_COMPILE_LEVEL >= STICKY_LOG_LEVEL_ERROR
    return ESP_LOG_ERROR;
#else
    return ESP_LOG_NONE;
#endif
}

}  // namespace

void app_log_init()
{
#if STICKY_LOG_COMPILE_LEVEL >= STICKY_LOG_LEVEL_DEBUG
    esp_log_level_set("*", ESP_LOG_INFO);
#elif STICKY_LOG_COMPILE_LEVEL >= STICKY_LOG_LEVEL_INFO
    esp_log_level_set("*", ESP_LOG_WARN);
#else
    esp_log_level_set("*", application_log_level());
#endif
}

void app_log_register_tag(const char *tag)
{
    if (tag != nullptr) {
        esp_log_level_set(tag, application_log_level());
    }
}

