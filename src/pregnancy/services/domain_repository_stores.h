#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "domain_repository_records.h"
#include "checkup_service.h"
#include "kick_service.h"
#include "reminder_service.h"
#include "sd_repository_adapter.h"
#include "weight_service.h"

namespace pregnancy_repository {

namespace detail {

enum class CollectionOperation : uint8_t {
    None = 0,
    List,
    Read,
    Upsert,
    Erase,
};

template <typename Item, typename Service, typename Traits,
          size_t Capacity = Service::kCapacity>
class RepositoryCollectionStore {
  public:
    sticky_storage::SdRepositoryAdapterStatus begin_load()
    {
        if (pending()) return sticky_storage::SdRepositoryAdapterStatus::Busy;
        adapter_.reset();
        service_.clear();
        ready_ = false;
        revisions_ = {};
        revision_count_ = 0U;
        page_ = {};
        page_index_ = 0U;
        high_id_ = 0U;
        return begin_list(0U);
    }

    sticky_storage::SdRepositoryAdapterStatus begin_upsert(Item item)
    {
        using sticky_storage::SdRepositoryAdapterStatus;
        if (pending()) return SdRepositoryAdapterStatus::Busy;
        if (!Traits::prepare(service_, item)) {
            status_ = SdRepositoryAdapterStatus::InvalidArgument;
            return status_;
        }
        if (service_.find(item.id) == nullptr && service_.count() >= Capacity) {
            status_ = SdRepositoryAdapterStatus::CapacityReached;
            return status_;
        }
        const uint32_t expected_revision = revision_for(item.id);
        if ((service_.find(item.id) == nullptr) != (expected_revision == 0U) ||
            !Traits::key(item.id, key_) ||
            Traits::encode(item, payload_) != RecordCodecStatus::Ok) {
            status_ = SdRepositoryAdapterStatus::InvalidArgument;
            return status_;
        }
        pending_item_ = item;
        status_ = adapter_.begin_upsert(key_, expected_revision,
                                        payload_.bytes.data(), payload_.size);
        operation_ = status_ == SdRepositoryAdapterStatus::Pending
                         ? CollectionOperation::Upsert
                         : CollectionOperation::None;
        return status_;
    }

    sticky_storage::SdRepositoryAdapterStatus begin_erase(uint32_t id)
    {
        using sticky_storage::SdRepositoryAdapterStatus;
        if (pending()) return SdRepositoryAdapterStatus::Busy;
        const uint32_t revision = revision_for(id);
        if (service_.find(id) == nullptr || revision == 0U ||
            !Traits::key(id, key_)) {
            status_ = SdRepositoryAdapterStatus::InvalidArgument;
            return status_;
        }
        pending_id_ = id;
        status_ = adapter_.begin_erase(key_, revision);
        operation_ = status_ == SdRepositoryAdapterStatus::Pending
                         ? CollectionOperation::Erase
                         : CollectionOperation::None;
        return status_;
    }

    sticky_storage::SdRepositoryAdapterStatus poll()
    {
        using sticky_storage::SdRepositoryAdapterStatus;
        if (!pending()) return status_;
        status_ = adapter_.poll();
        if (status_ == SdRepositoryAdapterStatus::Pending) return status_;
        if (status_ != SdRepositoryAdapterStatus::Ok) {
            operation_ = CollectionOperation::None;
            return status_;
        }

        if (operation_ == CollectionOperation::List) {
            page_ = adapter_.result().outcome.page;
            page_index_ = 0U;
            return advance_load();
        }
        if (operation_ == CollectionOperation::Read) {
            const sticky_storage::RepositoryRecord &record =
                adapter_.result().outcome.record;
            Item item = {};
            if (record.revision != pending_revision_ || record.tombstone ||
                Traits::decode(record.payload.data(), record.payload_size,
                               item) != RecordCodecStatus::Ok ||
                item.id != pending_id_ || !Traits::insert(service_, item) ||
                !remember_revision(item.id, record.revision)) {
                status_ = SdRepositoryAdapterStatus::Corrupt;
                operation_ = CollectionOperation::None;
                return status_;
            }
            return advance_load();
        }
        if (operation_ == CollectionOperation::Upsert) {
            const uint32_t revision =
                adapter_.result().outcome.batch.revisions[0];
            if (revision == 0U || !Traits::apply(service_, pending_item_) ||
                !remember_revision(pending_item_.id, revision)) {
                status_ = SdRepositoryAdapterStatus::Corrupt;
            }
            operation_ = CollectionOperation::None;
            return status_;
        }
        if (operation_ == CollectionOperation::Erase) {
            if (!service_.remove(pending_id_)) {
                status_ = SdRepositoryAdapterStatus::Corrupt;
            } else {
                forget_revision(pending_id_);
            }
            operation_ = CollectionOperation::None;
            return status_;
        }
        status_ = SdRepositoryAdapterStatus::Corrupt;
        operation_ = CollectionOperation::None;
        return status_;
    }

    bool pending() const { return operation_ != CollectionOperation::None; }
    bool ready() const { return ready_ && adapter_.binding_current(); }
    sticky_storage::SdRepositoryAdapterStatus status() const { return status_; }
    const Service &service() const { return service_; }
    uint32_t revision_for(uint32_t id) const
    {
        for (size_t index = 0U; index < revision_count_; ++index) {
            if (revisions_[index].id == id) return revisions_[index].revision;
        }
        return 0U;
    }

  private:
    struct RevisionEntry {
        uint32_t id = 0U;
        uint32_t revision = 0U;
    };

    sticky_storage::SdRepositoryAdapterStatus begin_list(uint32_t cursor)
    {
        status_ = adapter_.begin_list(Traits::kind, cursor, true);
        operation_ = status_ ==
                             sticky_storage::SdRepositoryAdapterStatus::Pending
                         ? CollectionOperation::List
                         : CollectionOperation::None;
        return status_;
    }

    sticky_storage::SdRepositoryAdapterStatus advance_load()
    {
        using sticky_storage::SdRepositoryAdapterStatus;
        while (page_index_ < page_.count) {
            const sticky_storage::RepositoryPageEntry &entry =
                page_.entries[page_index_++];
            uint32_t id = 0U;
            if (!numeric_record_id(entry.record_id, id)) {
                status_ = SdRepositoryAdapterStatus::Corrupt;
                operation_ = CollectionOperation::None;
                return status_;
            }
            if (id > high_id_) high_id_ = id;
            if (entry.tombstone) continue;
            if (entry.revision == 0U || !Traits::key(id, key_)) {
                status_ = SdRepositoryAdapterStatus::Corrupt;
                operation_ = CollectionOperation::None;
                return status_;
            }
            pending_id_ = id;
            pending_revision_ = entry.revision;
            status_ = adapter_.begin_read(key_);
            operation_ = status_ == SdRepositoryAdapterStatus::Pending
                             ? CollectionOperation::Read
                             : CollectionOperation::None;
            return status_;
        }
        if (page_.has_more) {
            if (page_.next_cursor == 0U) {
                status_ = SdRepositoryAdapterStatus::Corrupt;
                operation_ = CollectionOperation::None;
                return status_;
            }
            return begin_list(page_.next_cursor);
        }
        if (!service_.set_next_id_floor(static_cast<uint64_t>(high_id_) + 1U)) {
            status_ = SdRepositoryAdapterStatus::Corrupt;
        } else {
            status_ = SdRepositoryAdapterStatus::Ok;
            ready_ = true;
        }
        operation_ = CollectionOperation::None;
        return status_;
    }

    bool remember_revision(uint32_t id, uint32_t revision)
    {
        if (id == 0U || revision == 0U) return false;
        for (size_t index = 0U; index < revision_count_; ++index) {
            if (revisions_[index].id == id) {
                revisions_[index].revision = revision;
                return true;
            }
        }
        if (revision_count_ >= revisions_.size()) return false;
        revisions_[revision_count_++] = {id, revision};
        return true;
    }

    void forget_revision(uint32_t id)
    {
        for (size_t index = 0U; index < revision_count_; ++index) {
            if (revisions_[index].id != id) continue;
            for (size_t move = index + 1U; move < revision_count_; ++move)
                revisions_[move - 1U] = revisions_[move];
            revisions_[--revision_count_] = {};
            return;
        }
    }

    sticky_storage::SdRepositoryAdapter adapter_ = {};
    Service service_ = {};
    std::array<RevisionEntry, Capacity> revisions_ = {};
    size_t revision_count_ = 0U;
    sticky_storage::RepositoryPage page_ = {};
    size_t page_index_ = 0U;
    uint32_t high_id_ = 0U;
    bool ready_ = false;
    uint32_t pending_id_ = 0U;
    uint32_t pending_revision_ = 0U;
    sticky_storage::RepositoryKey key_ = {};
    EncodedPayload payload_ = {};
    Item pending_item_ = {};
    CollectionOperation operation_ = CollectionOperation::None;
    sticky_storage::SdRepositoryAdapterStatus status_ =
        sticky_storage::SdRepositoryAdapterStatus::Idle;
};

struct ReminderTraits {
    static constexpr sticky_storage::RepositoryKind kind =
        sticky_storage::RepositoryKind::Reminder;
    static bool key(uint32_t id, sticky_storage::RepositoryKey &output)
    { return reminder_key(id, output); }
    static RecordCodecStatus encode(const Reminder &item, EncodedPayload &payload)
    { return encode_reminder(item, payload); }
    static RecordCodecStatus decode(const uint8_t *data, size_t size,
                                    Reminder &item)
    { return decode_reminder(data, size, item); }
    static bool prepare(const ReminderService &service, Reminder &item)
    {
        if (item.id == 0U) item.id = service.next_id();
        return reminder_valid(item);
    }
    static bool insert(ReminderService &service, const Reminder &item)
    { return service.create(item); }
    static bool apply(ReminderService &service, const Reminder &item)
    {
        return service.find(item.id) == nullptr ? service.create(item)
                                                : service.update(item);
    }
};

struct CheckupTraits {
    static constexpr sticky_storage::RepositoryKind kind =
        sticky_storage::RepositoryKind::Checkup;
    static bool key(uint32_t id, sticky_storage::RepositoryKey &output)
    { return checkup_key(id, output); }
    static RecordCodecStatus encode(const Checkup &item, EncodedPayload &payload)
    { return encode_checkup(item, payload); }
    static RecordCodecStatus decode(const uint8_t *data, size_t size,
                                    Checkup &item)
    { return decode_checkup(data, size, item); }
    static bool prepare(const CheckupService &service, Checkup &item)
    {
        if (item.id == 0U) item.id = service.next_id();
        return checkup_valid(item);
    }
    static bool insert(CheckupService &service, const Checkup &item)
    { return service.create(item); }
    static bool apply(CheckupService &service, const Checkup &item)
    {
        return service.find(item.id) == nullptr ? service.create(item)
                                                : service.update(item);
    }
};

struct KickTraits {
    static constexpr sticky_storage::RepositoryKind kind = sticky_storage::RepositoryKind::Kicks;
    static bool key(uint32_t id, sticky_storage::RepositoryKey &output)
    { return kick_key(id, output); }
    static RecordCodecStatus encode(const KickSession &item, EncodedPayload &payload)
    { return encode_kick(item, payload); }
    static RecordCodecStatus decode(const uint8_t *data, size_t size, KickSession &item)
    { return decode_kick(data, size, item); }
    static bool prepare(const KickService &service, KickSession &item)
    {
        if (item.id == 0U) item.id = service.next_id();
        return kick_session_valid(item);
    }
    static bool insert(KickService &service, const KickSession &item)
    { return service.upsert(item); }
    static bool apply(KickService &service, const KickSession &item)
    { return service.upsert(item); }
};

struct WeightTraits {
    static constexpr sticky_storage::RepositoryKind kind =
        sticky_storage::RepositoryKind::Weight;
    static bool key(uint32_t id, sticky_storage::RepositoryKey &output)
    { return weight_key(id, output); }
    static RecordCodecStatus encode(const WeightRecord &item,
                                    EncodedPayload &payload)
    { return encode_weight(item, payload); }
    static RecordCodecStatus decode(const uint8_t *data, size_t size,
                                    WeightRecord &item)
    { return decode_weight(data, size, item); }
    static bool prepare(const WeightService &service, WeightRecord &item)
    {
        const WeightRecord *same_date = service.find_date(item.date);
        if (same_date != nullptr) item.id = same_date->id;
        else if (item.id == 0U) item.id = service.next_id();
        return weight_record_valid(item);
    }
    static bool insert(WeightService &service, const WeightRecord &item)
    { return service.find_date(item.date) == nullptr && service.find(item.id) == nullptr && service.upsert(item); }
    static bool apply(WeightService &service, const WeightRecord &item)
    { return service.upsert(item); }
};

}  // namespace detail

using ReminderRepositoryStore =
    detail::RepositoryCollectionStore<Reminder, ReminderService,
                                      detail::ReminderTraits>;
using CheckupRepositoryStore =
    detail::RepositoryCollectionStore<Checkup, CheckupService,
                                      detail::CheckupTraits>;
using KickRepositoryStore =
    detail::RepositoryCollectionStore<KickSession, KickService, detail::KickTraits>;
using WeightRecordRepositoryStore =
    detail::RepositoryCollectionStore<WeightRecord, WeightService,
                                      detail::WeightTraits>;

class WeightRepositoryStore {
  public:
    sticky_storage::SdRepositoryAdapterStatus begin_load();
    sticky_storage::SdRepositoryAdapterStatus begin_upsert(WeightRecord record);
    sticky_storage::SdRepositoryAdapterStatus begin_erase(uint32_t id);
    sticky_storage::SdRepositoryAdapterStatus begin_save_preferences(
        const WeightProfileSettings &settings);
    sticky_storage::SdRepositoryAdapterStatus poll();

    bool pending() const;
    bool ready() const { return ready_ && records_.ready() && preferences_adapter_.binding_current(); }
    sticky_storage::SdRepositoryAdapterStatus status() const { return status_; }
    const WeightService &service() const { return records_.service(); }
    const WeightProfileSettings &settings() const { return settings_; }

  private:
    enum class Operation : uint8_t {
        None = 0,
        LoadRecords,
        LoadPreferences,
        RecordMutation,
        SavePreferences,
    };

    WeightRecordRepositoryStore records_ = {};
    sticky_storage::SdRepositoryAdapter preferences_adapter_ = {};
    EncodedPayload payload_ = {};
    WeightProfileSettings settings_ = {};
    WeightProfileSettings pending_settings_ = {};
    uint32_t preferences_revision_ = 0U;
    bool ready_ = false;
    Operation operation_ = Operation::None;
    sticky_storage::SdRepositoryAdapterStatus status_ =
        sticky_storage::SdRepositoryAdapterStatus::Idle;
};

class DevicePreferencesRepositoryStore {
  public:
    sticky_storage::SdRepositoryAdapterStatus begin_load();
    sticky_storage::SdRepositoryAdapterStatus begin_save(
        const StickyDeviceSettings &settings);
    sticky_storage::SdRepositoryAdapterStatus poll();

    bool pending() const { return operation_ != Operation::None; }
    bool ready() const { return ready_ && adapter_.binding_current(); }
    bool found() const { return found_ && ready(); }
    const StickyDeviceSettings &settings() const { return settings_; }
    sticky_storage::SdRepositoryAdapterStatus status() const { return status_; }

  private:
    enum class Operation : uint8_t { None = 0, Load, Save };
    sticky_storage::SdRepositoryAdapter adapter_ = {};
    EncodedPayload payload_ = {};
    StickyDeviceSettings settings_ = {};
    StickyDeviceSettings pending_settings_ = {};
    uint32_t revision_ = 0U;
    bool ready_ = false;
    bool found_ = false;
    Operation operation_ = Operation::None;
    sticky_storage::SdRepositoryAdapterStatus status_ =
        sticky_storage::SdRepositoryAdapterStatus::Idle;
};

class NetworkAuthorizationRepositoryStore {
  public:
    sticky_storage::SdRepositoryAdapterStatus begin_load();
    sticky_storage::SdRepositoryAdapterStatus begin_save(
        const NetworkAuthorizationSettings &settings);
    sticky_storage::SdRepositoryAdapterStatus poll();

    bool pending() const { return operation_ != Operation::None; }
    bool ready() const { return ready_ && adapter_.binding_current(); }
    bool found() const { return found_ && ready(); }
    const NetworkAuthorizationSettings &settings() const { return settings_; }
    uint32_t revision() const { return revision_; }
    sticky_storage::SdRepositoryAdapterStatus status() const { return status_; }

  private:
    enum class Operation : uint8_t { None = 0, Load, Save };
    sticky_storage::SdRepositoryAdapter adapter_ = {};
    EncodedPayload payload_ = {};
    NetworkAuthorizationSettings settings_ = {};
    NetworkAuthorizationSettings pending_settings_ = {};
    uint32_t revision_ = 0U;
    bool ready_ = false;
    bool found_ = false;
    Operation operation_ = Operation::None;
    sticky_storage::SdRepositoryAdapterStatus status_ =
        sticky_storage::SdRepositoryAdapterStatus::Idle;
};

}  // namespace pregnancy_repository

namespace pregnancy_repository {

ReminderRepositoryStore &reminder_repository_store();
CheckupRepositoryStore &checkup_repository_store();
WeightRepositoryStore &weight_repository_store();
KickRepositoryStore &kick_repository_store();
DevicePreferencesRepositoryStore &device_preferences_repository_store();
NetworkAuthorizationRepositoryStore &network_authorization_repository_store();

}  // namespace pregnancy_repository
