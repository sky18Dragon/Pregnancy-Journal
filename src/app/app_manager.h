#pragma once

#include <cstddef>

#include "app_descriptor.h"

class StickyAppManager {
public:
    StickyAppManager(const StickyAppDescriptor *apps,
                     size_t count,
                     StickyAppId default_app);

    esp_err_t start(Canvas &canvas, StickyAppId requested);
    esp_err_t switch_to(StickyAppId requested);
    esp_err_t return_home();
    esp_err_t pause_current();
    esp_err_t resume_current();

    const StickyAppDescriptor *find(StickyAppId id) const;
    const StickyAppDescriptor *current() const;
    StickyAppId current_id() const;
    StickyAppId sanitize(StickyAppId requested) const;
    bool started(StickyAppId id) const;

private:
    static constexpr size_t kMaximumApps = 8U;

    size_t index_of(StickyAppId id) const;
    esp_err_t activate(size_t index);

    const StickyAppDescriptor *apps_;
    size_t count_;
    StickyAppId default_app_;
    Canvas *canvas_ = nullptr;
    size_t current_index_ = kMaximumApps;
    bool started_[kMaximumApps] = {};
};
