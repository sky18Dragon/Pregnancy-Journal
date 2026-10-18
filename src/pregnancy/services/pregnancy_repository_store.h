#pragma once

#include <cstdint>

#include "domain_repository_records.h"
#include "sd_repository_adapter.h"

namespace pregnancy_repository {

enum class PregnancyStoreOperation : uint8_t {
    None = 0,
    Load,
    Save,
    Revalidate,
};

// Async single-record store. Keep the instance in static storage so codec and
// repository workspaces do not consume the application task stack.
class PregnancyRepositoryStore {
  public:
    // Cold read: forgets the held profile first, so nothing is shown until the
    // card has answered.
    sticky_storage::SdRepositoryAdapterStatus begin_load();
    // Re-reads the record while keeping the held profile readable. Only a
    // profile that is still bound to the current card and dataset (ready())
    // is kept; otherwise this is exactly begin_load(). A failed re-read
    // forgets the held profile, so unverified data is never shown after an
    // error. changed() reports whether the card disagreed with the held copy.
    sticky_storage::SdRepositoryAdapterStatus begin_revalidate();
    // True when the last completed Load/Revalidate left found()/profile()
    // different from what was held when it began. A cold Load holds nothing
    // beforehand, so it always reports a change.
    bool changed() const { return changed_; }
    sticky_storage::SdRepositoryAdapterStatus begin_save(
        const PregnancyProfile &profile);
    sticky_storage::SdRepositoryAdapterStatus poll();

    bool pending() const { return adapter_.pending(); }
    bool ready() const { return ready_ && adapter_.binding_current(); }
    bool found() const { return found_ && ready(); }
    const PregnancyProfile &profile() const { return profile_; }
    uint32_t revision() const { return revision_; }
    PregnancyStoreOperation operation() const { return operation_; }
    sticky_storage::SdRepositoryAdapterStatus status() const {
        return status_;
    }

  private:
    void poll_revalidate();

    sticky_storage::SdRepositoryAdapter adapter_ = {};
    EncodedPayload payload_ = {};
    PregnancyProfile profile_ = {};
    PregnancyProfile pending_profile_ = {};
    uint32_t revision_ = 0U;
    bool found_ = false;
    bool ready_ = false;
    bool changed_ = false;
    PregnancyStoreOperation operation_ = PregnancyStoreOperation::None;
    sticky_storage::SdRepositoryAdapterStatus status_ =
        sticky_storage::SdRepositoryAdapterStatus::Idle;
};

}  // namespace pregnancy_repository

namespace pregnancy_repository {

PregnancyRepositoryStore &pregnancy_repository_store();

}  // namespace pregnancy_repository
