#include "pregnancy_repository_store.h"

namespace pregnancy_repository {

using sticky_storage::RepositoryKey;
using sticky_storage::SdRepositoryAdapterStatus;

SdRepositoryAdapterStatus PregnancyRepositoryStore::begin_load()
{
    if (pending()) return SdRepositoryAdapterStatus::Busy;
    RepositoryKey key = {};
    if (!pregnancy_profile_key(key)) {
        status_ = SdRepositoryAdapterStatus::InvalidArgument;
        return status_;
    }
    adapter_.reset();
    found_ = false;
    ready_ = false;
    profile_ = {};
    revision_ = 0U;
    status_ = adapter_.begin_read(key);
    operation_ = status_ == SdRepositoryAdapterStatus::Pending
                     ? PregnancyStoreOperation::Load
                     : PregnancyStoreOperation::None;
    return status_;
}

SdRepositoryAdapterStatus PregnancyRepositoryStore::begin_save(
    const PregnancyProfile &profile)
{
    if (pending()) return SdRepositoryAdapterStatus::Busy;
    RepositoryKey key = {};
    if (!pregnancy_profile_key(key) ||
        encode_pregnancy_profile(profile, payload_) != RecordCodecStatus::Ok) {
        status_ = SdRepositoryAdapterStatus::InvalidArgument;
        return status_;
    }
    pending_profile_ = profile;
    status_ = adapter_.begin_upsert(
        key, revision_, payload_.bytes.data(), payload_.size);
    operation_ = status_ == SdRepositoryAdapterStatus::Pending
                     ? PregnancyStoreOperation::Save
                     : PregnancyStoreOperation::None;
    return status_;
}

SdRepositoryAdapterStatus PregnancyRepositoryStore::poll()
{
    status_ = adapter_.poll();
    if (status_ == SdRepositoryAdapterStatus::Pending) return status_;
    if (operation_ == PregnancyStoreOperation::Load &&
        status_ == SdRepositoryAdapterStatus::NotFound) {
        found_ = false;
        revision_ = 0U;
        ready_ = true;
        status_ = SdRepositoryAdapterStatus::Ok;
    } else if (operation_ == PregnancyStoreOperation::Load &&
               status_ == SdRepositoryAdapterStatus::Ok) {
        const sticky_storage::RepositoryRecord &record =
            adapter_.result().outcome.record;
        if (decode_pregnancy_profile(record.payload.data(),
                                     record.payload_size,
                                     profile_) != RecordCodecStatus::Ok) {
            status_ = SdRepositoryAdapterStatus::Corrupt;
            found_ = false;
        } else {
            revision_ = record.revision;
            found_ = true;
            ready_ = true;
        }
    } else if (operation_ == PregnancyStoreOperation::Save &&
               status_ == SdRepositoryAdapterStatus::Ok) {
        revision_ = adapter_.result().outcome.batch.revisions[0];
        if (revision_ == 0U) {
            status_ = SdRepositoryAdapterStatus::Corrupt;
        } else {
            profile_ = pending_profile_;
            found_ = true;
            ready_ = true;
        }
    }
    if (status_ != SdRepositoryAdapterStatus::Pending) {
        operation_ = PregnancyStoreOperation::None;
    }
    return status_;
}

PregnancyRepositoryStore &pregnancy_repository_store()
{
    static PregnancyRepositoryStore store;
    return store;
}

}  // namespace pregnancy_repository
