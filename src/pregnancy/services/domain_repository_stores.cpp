#include "domain_repository_stores.h"

namespace pregnancy_repository {

using sticky_storage::RepositoryKey;
using sticky_storage::SdRepositoryAdapterStatus;

SdRepositoryAdapterStatus WeightRepositoryStore::begin_load()
{
    if (pending()) return SdRepositoryAdapterStatus::Busy;
    preferences_adapter_.reset();
    settings_ = {};
    ready_ = false;
    preferences_revision_ = 0U;
    status_ = records_.begin_load();
    operation_ = status_ == SdRepositoryAdapterStatus::Pending
                     ? Operation::LoadRecords
                     : Operation::None;
    return status_;
}

SdRepositoryAdapterStatus WeightRepositoryStore::begin_upsert(
    WeightRecord record)
{
    if (pending()) return SdRepositoryAdapterStatus::Busy;
    status_ = records_.begin_upsert(record);
    operation_ = status_ == SdRepositoryAdapterStatus::Pending
                     ? Operation::RecordMutation
                     : Operation::None;
    return status_;
}

SdRepositoryAdapterStatus WeightRepositoryStore::begin_erase(uint32_t id)
{
    if (pending()) return SdRepositoryAdapterStatus::Busy;
    status_ = records_.begin_erase(id);
    operation_ = status_ == SdRepositoryAdapterStatus::Pending
                     ? Operation::RecordMutation
                     : Operation::None;
    return status_;
}

SdRepositoryAdapterStatus WeightRepositoryStore::begin_save_preferences(
    const WeightProfileSettings &settings)
{
    if (pending()) return SdRepositoryAdapterStatus::Busy;
    RepositoryKey key = {};
    if (!weight_preferences_key(key) ||
        encode_weight_preferences(settings, payload_) != RecordCodecStatus::Ok) {
        status_ = SdRepositoryAdapterStatus::InvalidArgument;
        return status_;
    }
    pending_settings_ = settings;
    status_ = preferences_adapter_.begin_upsert(
        key, preferences_revision_, payload_.bytes.data(), payload_.size);
    operation_ = status_ == SdRepositoryAdapterStatus::Pending
                     ? Operation::SavePreferences
                     : Operation::None;
    return status_;
}

SdRepositoryAdapterStatus WeightRepositoryStore::poll()
{
    if (operation_ == Operation::None) return status_;
    if (operation_ == Operation::LoadRecords ||
        operation_ == Operation::RecordMutation) {
        status_ = records_.poll();
        if (status_ == SdRepositoryAdapterStatus::Pending) return status_;
        if (status_ != SdRepositoryAdapterStatus::Ok ||
            operation_ == Operation::RecordMutation) {
            operation_ = Operation::None;
            return status_;
        }
        RepositoryKey key = {};
        if (!weight_preferences_key(key)) {
            status_ = SdRepositoryAdapterStatus::InvalidArgument;
            operation_ = Operation::None;
            return status_;
        }
        status_ = preferences_adapter_.begin_read(key);
        operation_ = status_ == SdRepositoryAdapterStatus::Pending
                         ? Operation::LoadPreferences
                         : Operation::None;
        return status_;
    }

    status_ = preferences_adapter_.poll();
    if (status_ == SdRepositoryAdapterStatus::Pending) return status_;
    if (operation_ == Operation::LoadPreferences) {
        if (status_ == SdRepositoryAdapterStatus::NotFound) {
            settings_ = {};
            preferences_revision_ = 0U;
            status_ = SdRepositoryAdapterStatus::Ok;
        } else if (status_ == SdRepositoryAdapterStatus::Ok) {
            const sticky_storage::RepositoryRecord &record =
                preferences_adapter_.result().outcome.record;
            if (decode_weight_preferences(record.payload.data(),
                                          record.payload_size,
                                          settings_) != RecordCodecStatus::Ok) {
                status_ = SdRepositoryAdapterStatus::Corrupt;
            } else {
                preferences_revision_ = record.revision;
            }
        }
        if (status_ == SdRepositoryAdapterStatus::Ok) ready_ = true;
    } else if (operation_ == Operation::SavePreferences &&
               status_ == SdRepositoryAdapterStatus::Ok) {
        const uint32_t revision =
            preferences_adapter_.result().outcome.batch.revisions[0];
        if (revision == 0U) {
            status_ = SdRepositoryAdapterStatus::Corrupt;
        } else {
            preferences_revision_ = revision;
            settings_ = pending_settings_;
        }
    }
    operation_ = Operation::None;
    return status_;
}

bool WeightRepositoryStore::pending() const
{
    return operation_ != Operation::None;
}

SdRepositoryAdapterStatus DevicePreferencesRepositoryStore::begin_load()
{
    if (pending()) return SdRepositoryAdapterStatus::Busy;
    RepositoryKey key = {};
    if (!device_preferences_key(key)) {
        status_ = SdRepositoryAdapterStatus::InvalidArgument;
        return status_;
    }
    adapter_.reset();
    settings_ = {};
    revision_ = 0U;
    ready_ = false;
    found_ = false;
    status_ = adapter_.begin_read(key);
    operation_ = status_ == SdRepositoryAdapterStatus::Pending
                     ? Operation::Load
                     : Operation::None;
    return status_;
}

SdRepositoryAdapterStatus DevicePreferencesRepositoryStore::begin_save(
    const StickyDeviceSettings &settings)
{
    if (pending()) return SdRepositoryAdapterStatus::Busy;
    RepositoryKey key = {};
    if (!device_preferences_key(key) ||
        encode_device_preferences(settings, payload_) != RecordCodecStatus::Ok) {
        status_ = SdRepositoryAdapterStatus::InvalidArgument;
        return status_;
    }
    pending_settings_ = settings;
    status_ = adapter_.begin_upsert(key, revision_, payload_.bytes.data(),
                                    payload_.size);
    operation_ = status_ == SdRepositoryAdapterStatus::Pending
                     ? Operation::Save
                     : Operation::None;
    return status_;
}

SdRepositoryAdapterStatus DevicePreferencesRepositoryStore::poll()
{
    if (!pending()) return status_;
    status_ = adapter_.poll();
    if (status_ == SdRepositoryAdapterStatus::Pending) return status_;
    if (operation_ == Operation::Load) {
        if (status_ == SdRepositoryAdapterStatus::NotFound) {
            settings_ = {};
            revision_ = 0U;
            found_ = false;
            ready_ = true;
            status_ = SdRepositoryAdapterStatus::Ok;
        } else if (status_ == SdRepositoryAdapterStatus::Ok) {
            const sticky_storage::RepositoryRecord &record =
                adapter_.result().outcome.record;
            if (decode_device_preferences(record.payload.data(),
                                          record.payload_size,
                                          settings_) != RecordCodecStatus::Ok) {
                status_ = SdRepositoryAdapterStatus::Corrupt;
            } else {
                revision_ = record.revision;
                found_ = true;
                ready_ = true;
            }
        }
    } else if (operation_ == Operation::Save &&
               status_ == SdRepositoryAdapterStatus::Ok) {
        const uint32_t revision = adapter_.result().outcome.batch.revisions[0];
        if (revision == 0U) {
            status_ = SdRepositoryAdapterStatus::Corrupt;
        } else {
            revision_ = revision;
            settings_ = pending_settings_;
            found_ = true;
            ready_ = true;
        }
    }
    operation_ = Operation::None;
    return status_;
}

SdRepositoryAdapterStatus NetworkAuthorizationRepositoryStore::begin_load()
{
    if (pending()) return SdRepositoryAdapterStatus::Busy;
    RepositoryKey key = {};
    if (!network_authorization_key(key)) {
        status_ = SdRepositoryAdapterStatus::InvalidArgument;
        return status_;
    }
    adapter_.reset();
    settings_ = {};
    revision_ = 0U;
    ready_ = false;
    found_ = false;
    status_ = adapter_.begin_read(key);
    operation_ = status_ == SdRepositoryAdapterStatus::Pending
                     ? Operation::Load
                     : Operation::None;
    return status_;
}

SdRepositoryAdapterStatus NetworkAuthorizationRepositoryStore::begin_save(
    const NetworkAuthorizationSettings &settings)
{
    if (pending()) return SdRepositoryAdapterStatus::Busy;
    RepositoryKey key = {};
    if (!network_authorization_key(key) ||
        encode_network_authorization(settings, payload_) !=
            RecordCodecStatus::Ok) {
        status_ = SdRepositoryAdapterStatus::InvalidArgument;
        return status_;
    }
    pending_settings_ = settings;
    status_ = adapter_.begin_upsert(key, revision_, payload_.bytes.data(),
                                    payload_.size);
    operation_ = status_ == SdRepositoryAdapterStatus::Pending
                     ? Operation::Save
                     : Operation::None;
    return status_;
}

SdRepositoryAdapterStatus NetworkAuthorizationRepositoryStore::poll()
{
    if (!pending()) return status_;
    status_ = adapter_.poll();
    if (status_ == SdRepositoryAdapterStatus::Pending) return status_;
    if (operation_ == Operation::Load) {
        if (status_ == SdRepositoryAdapterStatus::NotFound) {
            settings_ = {};
            revision_ = 0U;
            found_ = false;
            ready_ = true;
            status_ = SdRepositoryAdapterStatus::Ok;
        } else if (status_ == SdRepositoryAdapterStatus::Ok) {
            const sticky_storage::RepositoryRecord &record =
                adapter_.result().outcome.record;
            if (decode_network_authorization(record.payload.data(),
                                             record.payload_size,
                                             settings_) !=
                RecordCodecStatus::Ok) {
                status_ = SdRepositoryAdapterStatus::Corrupt;
            } else {
                revision_ = record.revision;
                found_ = true;
                ready_ = true;
            }
        }
    } else if (operation_ == Operation::Save &&
               status_ == SdRepositoryAdapterStatus::Ok) {
        const uint32_t revision = adapter_.result().outcome.batch.revisions[0];
        if (revision == 0U) {
            status_ = SdRepositoryAdapterStatus::Corrupt;
        } else {
            revision_ = revision;
            settings_ = pending_settings_;
            found_ = true;
            ready_ = true;
        }
    }
    operation_ = Operation::None;
    return status_;
}

ReminderRepositoryStore &reminder_repository_store()
{
    static ReminderRepositoryStore store;
    return store;
}

CheckupRepositoryStore &checkup_repository_store()
{
    static CheckupRepositoryStore store;
    return store;
}

KickRepositoryStore &kick_repository_store()
{
    static KickRepositoryStore store;
    return store;
}

WeightRepositoryStore &weight_repository_store()
{
    static WeightRepositoryStore store;
    return store;
}

DevicePreferencesRepositoryStore &device_preferences_repository_store()
{
    static DevicePreferencesRepositoryStore store;
    return store;
}

NetworkAuthorizationRepositoryStore &network_authorization_repository_store()
{
    static NetworkAuthorizationRepositoryStore store;
    return store;
}

}  // namespace pregnancy_repository
