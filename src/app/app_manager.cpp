#include "app_manager.h"

namespace {

constexpr size_t kNotFound = 8U;

}  // namespace

StickyAppManager::StickyAppManager(const StickyAppDescriptor *apps,
                                   size_t count,
                                   StickyAppId default_app)
    : apps_(apps),
      count_(count <= kMaximumApps ? count : kMaximumApps),
      default_app_(default_app)
{
}

size_t StickyAppManager::index_of(StickyAppId id) const
{
    for (size_t index = 0; index < count_; ++index) {
        if (apps_[index].id == id) {
            return index;
        }
    }
    return kNotFound;
}

const StickyAppDescriptor *StickyAppManager::find(StickyAppId id) const
{
    const size_t index = index_of(id);
    return index == kNotFound ? nullptr : &apps_[index];
}

StickyAppId StickyAppManager::sanitize(StickyAppId requested) const
{
    return find(requested) == nullptr ? default_app_ : requested;
}

esp_err_t StickyAppManager::activate(size_t index)
{
    if (index >= count_ || canvas_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    const StickyAppDescriptor &app = apps_[index];
    const esp_err_t result = started_[index]
                                 ? app.resume()
                                 : app.start(*canvas_);
    if (result == ESP_OK) {
        started_[index] = true;
        current_index_ = index;
    }
    return result;
}

esp_err_t StickyAppManager::start(Canvas &canvas, StickyAppId requested)
{
    canvas_ = &canvas;
    return activate(index_of(sanitize(requested)));
}

esp_err_t StickyAppManager::switch_to(StickyAppId requested)
{
    if (canvas_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    const size_t destination = index_of(sanitize(requested));
    if (destination == current_index_) {
        return ESP_OK;
    }
    const size_t previous = current_index_;
    if (previous < count_) {
        const esp_err_t pause_result = apps_[previous].pause();
        if (pause_result != ESP_OK) {
            return pause_result;
        }
    }
    const esp_err_t result = activate(destination);
    if (result != ESP_OK && previous < count_) {
        apps_[previous].resume();
        current_index_ = previous;
    }
    return result;
}

esp_err_t StickyAppManager::switch_from_paused_to(StickyAppId requested)
{
    if (canvas_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    const size_t destination = index_of(sanitize(requested));
    if (destination == current_index_) {
        return resume_current();
    }
    const size_t previous = current_index_;
    const esp_err_t result = activate(destination);
    if (result != ESP_OK && previous < count_) {
        apps_[previous].resume();
        current_index_ = previous;
    }
    return result;
}

esp_err_t StickyAppManager::return_home()
{
    return switch_to(default_app_);
}

esp_err_t StickyAppManager::pause_current()
{
    const StickyAppDescriptor *app = current();
    return app == nullptr ? ESP_ERR_INVALID_STATE : app->pause();
}

esp_err_t StickyAppManager::resume_current()
{
    const StickyAppDescriptor *app = current();
    return app == nullptr ? ESP_ERR_INVALID_STATE : app->resume();
}

const StickyAppDescriptor *StickyAppManager::current() const
{
    return current_index_ < count_ ? &apps_[current_index_] : nullptr;
}

StickyAppId StickyAppManager::current_id() const
{
    const StickyAppDescriptor *app = current();
    return app == nullptr ? default_app_ : app->id;
}

bool StickyAppManager::started(StickyAppId id) const
{
    const size_t index = index_of(id);
    return index != kNotFound && started_[index];
}
