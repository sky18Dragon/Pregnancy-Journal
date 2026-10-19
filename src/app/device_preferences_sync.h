#pragma once

#include <cstring>
#include "persistent_state.h"
#include "sd_repository_adapter.h"

// Coordinator-owned, asynchronous preference synchronization. A generation
// change discards old-card intentions; an uncertain same-card write reloads its
// revision before retrying the latest intention with bounded backoff.
class DevicePreferencesSync {
public:
    const StickyDeviceSettings &desired() const { return desired_; }
    bool saving_failed() const { return failed_; }
    void set_last_app(StickyAppId app) {
        if (bound_ && desired_.last_app != app) { desired_.last_app = app; dirty_ = true; }
    }
    void set_shortcut_target(StickyShortcutTarget target) {
        if (bound_ && desired_.shortcut_target != target) {
            desired_.shortcut_target = target; dirty_ = true;
        }
    }
    void set_kick_mode_pref(StickyKickModePref pref) {
        if (bound_ && desired_.kick_mode_pref != pref) {
            desired_.kick_mode_pref = pref; dirty_ = true;
        }
    }
    void toggle_language() {
        if (!bound_) return;
        desired_.language = desired_.language == UiLanguage::ChineseSimplified
                                ? UiLanguage::English : UiLanguage::ChineseSimplified;
        dirty_ = true;
    }

    template<class Store>
    void tick(Store &store, const sticky_storage::SdStorageServiceStatus &card,
              uint32_t now) {
        using Status = sticky_storage::SdRepositoryAdapterStatus;
        const bool same = generation_ != 0U && generation_ == card.mount_generation &&
            card.manifest_selected && std::strcmp(dataset_, card.selected_manifest.dataset_id) == 0;
        if (!same) {
            generation_ = card.mount_generation;
            std::memcpy(dataset_, card.selected_manifest.dataset_id, sizeof(dataset_));
            bound_ = false; dirty_ = false; reload_ = true; failed_ = false;
            retry_at_ = now; failures_ = 0U;
            obsolete_ = store.pending();
        }
        if (store.pending()) {
            const auto result = store.poll();
            if (result == Status::Pending) return;
            if (!obsolete_) {
                if (result == Status::Ok && store.ready()) {
                    if (loading_) {
                        if (!bound_) desired_ = store.settings();
                        bound_ = true; reload_ = false;
                    } else {
                        dirty_ = desired_.language != sent_.language ||
                                 desired_.last_app != sent_.last_app ||
                                 desired_.shortcut_target != sent_.shortcut_target ||
                                 desired_.kick_mode_pref != sent_.kick_mode_pref;
                    }
                    failed_ = false; failures_ = 0U;
                } else {
                    defer(now); reload_ = true;
                }
            }
            obsolete_ = false;
        }
        if (static_cast<int32_t>(now - retry_at_) < 0) return;
        if (sticky_storage::sd_repository_adapter_preflight(card, false) != Status::Ok)
            return;
        if (!store.ready()) reload_ = true;
        Status result = Status::Idle;
        if (reload_) {
            loading_ = true;
            result = store.begin_load();
        } else if (dirty_) {
            loading_ = false;
            sent_ = desired_;
            result = store.begin_save(sent_);
        } else return;
        if (result != Status::Pending) { defer(now); reload_ = true; }
    }
private:
    void defer(uint32_t now) {
        failed_ = true;
        if (failures_ < 4U) ++failures_;
        retry_at_ = now + (500U << failures_);
    }
    StickyDeviceSettings desired_ = {}, sent_ = {};
    uint32_t generation_ = 0U, retry_at_ = 0U;
    char dataset_[sticky_storage::kDatasetIdCharacters + 1U] = {};
    unsigned failures_ = 0U;
    bool bound_ = false, dirty_ = false, reload_ = true;
    bool loading_ = false, obsolete_ = false, failed_ = false;
};
