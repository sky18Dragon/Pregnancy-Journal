#include <array>
#include <cassert>
#include <cstring>

#include "sd_repository_adapter.h"
#include "domain_repository_stores.h"
#include "legacy_data_migration.h"
#include "pregnancy_repository_store.h"

namespace sticky_storage {
namespace {
SdStorageServiceStatus g_status = {};
SdStorageRequestHandle g_handle = {41U, 2U};
SdStorageRepositoryResult g_result = {};
bool g_pending_once = false;
// When not ESP_OK, the next repository read submission is refused with it.
esp_err_t g_read_submit_result = ESP_OK;
uint64_t g_transaction_id = UINT64_MAX;
RepositoryMutation g_mutation = {};
SdRepositoryRequestOperation g_operation = SdRepositoryRequestOperation::Read;
RepositoryKey g_read_key = {};
RepositoryPageRequest g_page_request = {};
}  // namespace

SdStorageServiceStatus sd_storage_service_status() { return g_status; }

esp_err_t sd_storage_service_submit_repository_read(
    uint32_t, const char *, const RepositoryKey &key, SdStorageRequestHandle &handle)
{
    if (g_read_submit_result != ESP_OK) return g_read_submit_result;
    g_operation = SdRepositoryRequestOperation::Read;
    g_read_key = key;
    handle = g_handle;
    return ESP_OK;
}

esp_err_t sd_storage_service_submit_repository_list(
    uint32_t, const char *, const RepositoryPageRequest &page,
    SdStorageRequestHandle &handle)
{
    g_operation = SdRepositoryRequestOperation::List;
    g_page_request = page;
    handle = g_handle;
    return ESP_OK;
}

esp_err_t sd_storage_service_submit_repository_commit(
    uint32_t, const char *, const RepositoryMutation &mutation,
    uint64_t transaction_id, SdStorageRequestHandle &handle)
{
    g_mutation = mutation;
    g_transaction_id = transaction_id;
    handle = g_handle;
    return ESP_OK;
}

esp_err_t sd_storage_service_submit_repository_erase(
    uint32_t, const char *, const RepositoryKey &, uint32_t,
    uint64_t transaction_id, SdStorageRequestHandle &handle)
{
    g_transaction_id = transaction_id;
    handle = g_handle;
    return ESP_OK;
}

esp_err_t sd_storage_service_collect_repository_result(
    const SdStorageRequestHandle &, SdStorageRepositoryResult &result)
{
    if (g_pending_once) {
        g_pending_once = false;
        return ESP_ERR_NOT_FINISHED;
    }
    result = g_result;
    return ESP_OK;
}
}  // namespace sticky_storage

namespace {
class FakeLegacySource final : public sticky_storage::LegacyMigrationSource {
  public:
    uint32_t slot_count() const override { return slots; }
    sticky_storage::LegacyMigrationReadStatus read(
        uint32_t slot,
        sticky_storage::LegacyMigrationRecord &record) override
    {
        if (slot == 0U) {
            record = migration_record;
            return sticky_storage::LegacyMigrationReadStatus::Record;
        }
        if (slot == 1U)
            return sticky_storage::LegacyMigrationReadStatus::Missing;
        return sticky_storage::LegacyMigrationReadStatus::Corrupt;
    }
    bool legacy_kicks_present() const override { return kicks; }
    bool clear_migrated_namespaces() override
    {
        cleared = true;
        return clear_result;
    }

    uint32_t slots = 3U;
    bool kicks = true;
    bool clear_result = true;
    bool cleared = false;
    sticky_storage::LegacyMigrationRecord migration_record = {};
};
}  // namespace

int main()
{
    using namespace sticky_storage;
    g_status.running = true;
    g_status.accepting_repository_requests = true;
    g_status.card_state = SdCardState::ReadyReadWrite;
    g_status.mount_generation = 3U;
    g_status.capacity_known = true;
    g_status.manifest_known = true;
    g_status.manifest_state = SdManifestProbeState::Ready;
    g_status.manifest_selected = true;
    std::strcpy(g_status.selected_manifest.dataset_id,
                "0123456789abcdef0123456789abcdef");
    g_status.recovery_known = true;
    g_status.recovery_view_ready = true;

    assert(sd_repository_adapter_preflight(g_status, true) ==
           SdRepositoryAdapterStatus::Ok);
    SdStorageServiceStatus unavailable = g_status;
    unavailable.card_state = SdCardState::Absent;
    assert(sd_repository_adapter_preflight(unavailable, false) ==
           SdRepositoryAdapterStatus::CardMissing);
    unavailable = g_status;
    unavailable.card_state = SdCardState::ReadyReadOnly;
    assert(sd_repository_adapter_preflight(unavailable, true) ==
           SdRepositoryAdapterStatus::CardReadOnly);
    assert(sd_repository_adapter_preflight(unavailable, false) ==
           SdRepositoryAdapterStatus::Ok);
    unavailable = g_status;
    unavailable.recovery_view_ready = false;
    assert(sd_repository_adapter_preflight(unavailable, false) ==
           SdRepositoryAdapterStatus::RecoveryUnavailable);

    RepositoryKey key = {};
    assert(repository_key_make(
        RepositoryKind::Reminder,
        "00000000000000000000000000000001", key));
    constexpr std::array<uint8_t, 3U> payload = {1U, 2U, 3U};
    SdRepositoryAdapter adapter;
    assert(adapter.begin_upsert(key, 0U, payload.data(), payload.size()) ==
           SdRepositoryAdapterStatus::Pending);
    assert(g_transaction_id == kRepositoryAutoTransactionId);
    assert(g_mutation.payload_size == payload.size());
    assert(std::memcmp(g_mutation.payload.data(), payload.data(),
                       payload.size()) == 0);
    assert(adapter.begin_read(key) == SdRepositoryAdapterStatus::Busy);

    g_result.outcome.status = SdRepositoryRequestStatus::Ok;
    g_result.outcome.batch.transaction_id = 9U;
    g_pending_once = true;
    assert(adapter.poll() == SdRepositoryAdapterStatus::Pending);
    assert(adapter.poll() == SdRepositoryAdapterStatus::Ok);
    assert(adapter.result().outcome.batch.transaction_id == 9U);

    g_status.card_state = SdCardState::Full;
    assert(adapter.begin_erase(key, 1U) ==
           SdRepositoryAdapterStatus::CardFull);
    g_status.card_state = SdCardState::ReadyReadWrite;
    assert(adapter.begin_list(RepositoryKind::Reminder, 0U, true) ==
           SdRepositoryAdapterStatus::Pending);
    g_result.outcome.status = SdRepositoryRequestStatus::RecoveryError;
    assert(adapter.poll() == SdRepositoryAdapterStatus::Corrupt);

    PregnancyProfile profile = {};
    assert(pregnancy_profile_from_due_date({2027U, 4U, 18U}, profile));
    pregnancy_repository::PregnancyRepositoryStore profile_store;
    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::NotFound;
    assert(profile_store.begin_load() == SdRepositoryAdapterStatus::Pending);
    assert(profile_store.poll() == SdRepositoryAdapterStatus::Ok);
    assert(!profile_store.found() && profile_store.revision() == 0U);

    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::Ok;
    g_result.outcome.batch.revisions[0] = 1U;
    assert(profile_store.begin_save(profile) ==
           SdRepositoryAdapterStatus::Pending);
    assert(!profile_store.found());
    assert(profile_store.poll() == SdRepositoryAdapterStatus::Ok);
    assert(profile_store.found() && profile_store.revision() == 1U);

    pregnancy_repository::EncodedPayload encoded = {};
    assert(pregnancy_repository::encode_pregnancy_profile(profile, encoded) ==
           pregnancy_repository::RecordCodecStatus::Ok);
    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::Ok;
    g_result.outcome.record.revision = 4U;
    g_result.outcome.record.payload_size = encoded.size;
    std::memcpy(g_result.outcome.record.payload.data(), encoded.bytes.data(),
                encoded.size);
    pregnancy_repository::PregnancyRepositoryStore reloaded;
    assert(reloaded.begin_load() == SdRepositoryAdapterStatus::Pending);
    assert(reloaded.poll() == SdRepositoryAdapterStatus::Ok);
    assert(reloaded.found() && reloaded.revision() == 4U);
    assert(reloaded.profile().estimated_due_date.year == 2027U);

    // Revalidate: the held profile stays readable while the card is asked,
    // and a change is only reported when the card disagrees with it.
    assert(reloaded.ready());
    assert(reloaded.begin_revalidate() == SdRepositoryAdapterStatus::Pending);
    assert(reloaded.operation() ==
           pregnancy_repository::PregnancyStoreOperation::Revalidate);
    assert(reloaded.found() && reloaded.profile().estimated_due_date.year == 2027U);
    assert(reloaded.poll() == SdRepositoryAdapterStatus::Ok);
    assert(!reloaded.changed() && reloaded.found() && reloaded.revision() == 4U);

    PregnancyProfile later = {};
    assert(pregnancy_profile_from_due_date({2027U, 5U, 2U}, later));
    pregnancy_repository::EncodedPayload later_encoded = {};
    assert(pregnancy_repository::encode_pregnancy_profile(later, later_encoded) ==
           pregnancy_repository::RecordCodecStatus::Ok);
    g_result.outcome.record.revision = 5U;
    g_result.outcome.record.payload_size = later_encoded.size;
    std::memcpy(g_result.outcome.record.payload.data(),
                later_encoded.bytes.data(), later_encoded.size);
    assert(reloaded.begin_revalidate() == SdRepositoryAdapterStatus::Pending);
    assert(reloaded.poll() == SdRepositoryAdapterStatus::Ok);
    assert(reloaded.changed() && reloaded.found() && reloaded.revision() == 5U);
    assert(reloaded.profile().estimated_due_date.month == 5U);

    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::NotFound;
    assert(reloaded.begin_revalidate() == SdRepositoryAdapterStatus::Pending);
    assert(reloaded.poll() == SdRepositoryAdapterStatus::Ok);
    assert(reloaded.changed() && !reloaded.found() && reloaded.ready() &&
           reloaded.revision() == 0U);
    assert(reloaded.begin_revalidate() == SdRepositoryAdapterStatus::Pending);
    assert(reloaded.poll() == SdRepositoryAdapterStatus::Ok);
    assert(!reloaded.changed() && !reloaded.found());

    // A failed re-read forgets the held profile instead of vouching for it,
    // and the next attempt is a cold load.
    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::Ok;
    g_result.outcome.record.revision = 6U;
    g_result.outcome.record.payload_size = encoded.size;
    std::memcpy(g_result.outcome.record.payload.data(), encoded.bytes.data(),
                encoded.size);
    assert(reloaded.begin_load() == SdRepositoryAdapterStatus::Pending);
    assert(reloaded.poll() == SdRepositoryAdapterStatus::Ok && reloaded.found());
    g_result.outcome.status = SdRepositoryRequestStatus::RecoveryError;
    assert(reloaded.begin_revalidate() == SdRepositoryAdapterStatus::Pending);
    assert(reloaded.poll() == SdRepositoryAdapterStatus::Corrupt);
    assert(!reloaded.found() && !reloaded.ready() && reloaded.changed());
    g_result.outcome.status = SdRepositoryRequestStatus::Ok;
    assert(reloaded.begin_revalidate() == SdRepositoryAdapterStatus::Pending);
    assert(reloaded.operation() ==
           pregnancy_repository::PregnancyStoreOperation::Load);
    assert(!reloaded.found());
    assert(reloaded.poll() == SdRepositoryAdapterStatus::Ok && reloaded.found());

    // A card or dataset change makes the held profile unusable at once, and
    // revalidate then reads cold rather than trusting it.
    g_status.mount_generation = 4U;
    assert(!reloaded.ready() && !reloaded.found());
    assert(reloaded.begin_revalidate() == SdRepositoryAdapterStatus::Pending);
    assert(reloaded.operation() ==
           pregnancy_repository::PregnancyStoreOperation::Load);
    assert(reloaded.poll() == SdRepositoryAdapterStatus::Ok && reloaded.found());
    g_status.mount_generation = 3U;
    assert(!reloaded.ready());

    // More revalidate edges, each on a store that holds revision 6.
    auto make_ok_record = [&](uint32_t revision,
                              const pregnancy_repository::EncodedPayload &p) {
        g_result = {};
        g_result.outcome.status = SdRepositoryRequestStatus::Ok;
        g_result.outcome.record.revision = revision;
        g_result.outcome.record.payload_size = p.size;
        std::memcpy(g_result.outcome.record.payload.data(), p.bytes.data(),
                    p.size);
    };
    pregnancy_repository::PregnancyRepositoryStore held;
    make_ok_record(6U, encoded);
    assert(held.begin_load() == SdRepositoryAdapterStatus::Pending);
    assert(held.poll() == SdRepositoryAdapterStatus::Ok && held.found());

    // The card cannot even be asked: the held profile is dropped, not kept.
    g_read_submit_result = ESP_ERR_NO_MEM;
    assert(held.begin_revalidate() == SdRepositoryAdapterStatus::Busy);
    g_read_submit_result = ESP_OK;
    assert(!held.ready() && !held.found() && held.changed());
    assert(held.operation() == pregnancy_repository::PregnancyStoreOperation::None);
    assert(held.begin_revalidate() == SdRepositoryAdapterStatus::Pending);
    assert(held.operation() == pregnancy_repository::PregnancyStoreOperation::Load);
    assert(held.poll() == SdRepositoryAdapterStatus::Ok && held.found());

    // A second request while one is pending is refused and the held profile
    // stays readable and unchanged.
    assert(held.begin_revalidate() == SdRepositoryAdapterStatus::Pending);
    assert(held.begin_revalidate() == SdRepositoryAdapterStatus::Busy);
    assert(held.found() && held.revision() == 6U &&
           held.profile().estimated_due_date.year == 2027U);
    assert(held.poll() == SdRepositoryAdapterStatus::Ok && !held.changed());

    // The card changes while the re-read is in flight: stale, so dropped.
    assert(held.begin_revalidate() == SdRepositoryAdapterStatus::Pending);
    g_status.mount_generation = 9U;
    assert(held.poll() == SdRepositoryAdapterStatus::StaleGeneration);
    assert(!held.ready() && !held.found() && held.changed());
    g_status.mount_generation = 3U;

    // A different revision with an undecodable payload is corrupt and dropped.
    make_ok_record(6U, encoded);
    assert(held.begin_load() == SdRepositoryAdapterStatus::Pending);
    assert(held.poll() == SdRepositoryAdapterStatus::Ok && held.found());
    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::Ok;
    g_result.outcome.record.revision = 7U;
    g_result.outcome.record.payload_size = 5U;
    std::memset(g_result.outcome.record.payload.data(), 0xFF, 5U);
    assert(held.begin_revalidate() == SdRepositoryAdapterStatus::Pending);
    assert(held.poll() == SdRepositoryAdapterStatus::Corrupt);
    assert(!held.ready() && !held.found() && held.changed());

    // The same revision is trusted without decoding again, so the held copy
    // stays even if the returned bytes were garbage.
    make_ok_record(6U, encoded);
    assert(held.begin_load() == SdRepositoryAdapterStatus::Pending);
    assert(held.poll() == SdRepositoryAdapterStatus::Ok && held.found());
    g_result.outcome.record.payload_size = 5U;
    std::memset(g_result.outcome.record.payload.data(), 0xFF, 5U);
    assert(held.begin_revalidate() == SdRepositoryAdapterStatus::Pending);
    assert(held.poll() == SdRepositoryAdapterStatus::Ok && !held.changed());
    assert(held.found() && held.profile().estimated_due_date.year == 2027U);

    // After a changed re-read a save expects the new revision, not the old one.
    make_ok_record(8U, later_encoded);
    assert(held.begin_revalidate() == SdRepositoryAdapterStatus::Pending);
    assert(held.poll() == SdRepositoryAdapterStatus::Ok && held.changed());
    assert(held.revision() == 8U);
    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::Ok;
    g_result.outcome.batch.revisions[0] = 9U;
    assert(held.begin_save(profile) == SdRepositoryAdapterStatus::Pending);
    assert(g_mutation.expected_revision == 8U);
    assert(held.poll() == SdRepositoryAdapterStatus::Ok && held.revision() == 9U);

    pregnancy_repository::ReminderRepositoryStore reminders;
    RepositoryKey reminder_key = {};
    assert(pregnancy_repository::reminder_key(2U, reminder_key));
    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::Ok;
    g_result.outcome.page.count = 2U;
    std::strcpy(g_result.outcome.page.entries[0].record_id,
                reminder_key.record_id);
    g_result.outcome.page.entries[0].revision = 3U;
    RepositoryKey deleted_key = {};
    assert(pregnancy_repository::reminder_key(9U, deleted_key));
    std::strcpy(g_result.outcome.page.entries[1].record_id,
                deleted_key.record_id);
    g_result.outcome.page.entries[1].revision = 4U;
    g_result.outcome.page.entries[1].tombstone = true;
    assert(reminders.begin_load() == SdRepositoryAdapterStatus::Pending);
    assert(reminders.poll() == SdRepositoryAdapterStatus::Pending);

    Reminder stored = {};
    stored.id = 2U;
    stored.date = {2027U, 1U, 2U};
    stored.hour = 8U;
    std::strcpy(stored.title, "STORED");
    pregnancy_repository::EncodedPayload reminder_payload = {};
    assert(pregnancy_repository::encode_reminder(stored, reminder_payload) ==
           pregnancy_repository::RecordCodecStatus::Ok);
    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::Ok;
    g_result.outcome.record.key = reminder_key;
    g_result.outcome.record.revision = 3U;
    g_result.outcome.record.payload_size = reminder_payload.size;
    std::memcpy(g_result.outcome.record.payload.data(),
                reminder_payload.bytes.data(), reminder_payload.size);
    assert(reminders.poll() == SdRepositoryAdapterStatus::Ok);
    assert(reminders.service().count() == 1U);
    assert(reminders.service().next_id() == 10U);

    Reminder created = {};
    created.date = {2027U, 1U, 3U};
    created.hour = 9U;
    std::strcpy(created.title, "NEW");
    assert(reminders.begin_upsert(created) ==
           SdRepositoryAdapterStatus::Pending);
    assert(g_mutation.expected_revision == 0U);
    uint32_t mutation_id = 0U;
    assert(pregnancy_repository::numeric_record_id(
        g_mutation.key.record_id, mutation_id));
    assert(mutation_id == 10U);
    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::Ok;
    g_result.outcome.batch.revisions[0] = 1U;
    assert(reminders.poll() == SdRepositoryAdapterStatus::Ok);
    assert(reminders.service().find(10U) != nullptr);

    assert(reminders.begin_erase(2U) == SdRepositoryAdapterStatus::Pending);
    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::Ok;
    g_result.outcome.batch.revisions[0] = 4U;
    assert(reminders.poll() == SdRepositoryAdapterStatus::Ok);
    assert(reminders.service().find(2U) == nullptr);

    pregnancy_repository::WeightRepositoryStore weights;
    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::Ok;
    assert(weights.begin_load() == SdRepositoryAdapterStatus::Pending);
    assert(weights.poll() == SdRepositoryAdapterStatus::Pending);
    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::NotFound;
    assert(weights.poll() == SdRepositoryAdapterStatus::Ok);
    assert(weights.settings().height_cm == 165U);

    WeightProfileSettings weight_settings = weights.settings();
    weight_settings.use_pounds = true;
    assert(weights.begin_save_preferences(weight_settings) ==
           SdRepositoryAdapterStatus::Pending);
    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::Ok;
    g_result.outcome.batch.revisions[0] = 1U;
    assert(weights.poll() == SdRepositoryAdapterStatus::Ok);
    assert(weights.settings().use_pounds);

    pregnancy_repository::DevicePreferencesRepositoryStore preferences;
    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::NotFound;
    assert(preferences.begin_load() == SdRepositoryAdapterStatus::Pending);
    assert(preferences.poll() == SdRepositoryAdapterStatus::Ok);
    assert(preferences.ready() && !preferences.found());
    StickyDeviceSettings device = preferences.settings();
    device.language = UiLanguage::ChineseSimplified;
    device.last_app = StickyAppId::Weight;
    assert(preferences.begin_save(device) ==
           SdRepositoryAdapterStatus::Pending);
    assert(preferences.settings().language == UiLanguage::English);
    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::Ok;
    g_result.outcome.batch.revisions[0] = 1U;
    assert(preferences.poll() == SdRepositoryAdapterStatus::Ok);
    assert(preferences.settings().language == UiLanguage::ChineseSimplified);
    assert(preferences.settings().last_app == StickyAppId::Weight);

    pregnancy_repository::NetworkAuthorizationRepositoryStore authorization;
    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::NotFound;
    assert(authorization.begin_load() == SdRepositoryAdapterStatus::Pending);
    assert(authorization.poll() == SdRepositoryAdapterStatus::Ok);
    assert(authorization.ready() && !authorization.found());
    NetworkAuthorizationSettings network = {};
    network.cloud_authorized = true;
    network.authorization_version = 1U;
    network.provider_config_version = 1U;
    std::strcpy(network.provider_id, "coze");
    std::strcpy(network.service_region, "cn");
    network.credentials.ciphertext_size = 4U;
    network.credentials.device_id[0] = 1U;
    network.credentials.nonce[0] = 2U;
    network.credentials.ciphertext[0] = 3U;
    network.credentials.tag[0] = 4U;
    assert(authorization.begin_save(network) ==
           SdRepositoryAdapterStatus::Pending);
    RepositoryKey authorization_key = {};
    assert(pregnancy_repository::network_authorization_key(
        authorization_key));
    assert(g_mutation.key.repository == authorization_key.repository);
    assert(std::strcmp(g_mutation.key.record_id,
                       authorization_key.record_id) == 0);
    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::Ok;
    g_result.outcome.batch.revisions[0] = 1U;
    assert(authorization.poll() == SdRepositoryAdapterStatus::Ok);
    assert(authorization.found() && authorization.revision() == 1U);
    assert(std::strcmp(authorization.settings().provider_id, "coze") == 0);

    pregnancy_repository::KickRepositoryStore kicks;
    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::Ok;
    assert(kicks.begin_load() == SdRepositoryAdapterStatus::Pending);
    assert(kicks.poll() == SdRepositoryAdapterStatus::Ok && kicks.ready());
    KickSession kick = {};
    kick.date = {2026U, 10U, 10U};
    kick.hour = 9U;
    kick.count = 12U;
    kick.duration_minutes = 5U;
    for (unsigned item = 0U; item < KickService::kCapacity; ++item) {
        assert(kicks.begin_upsert(kick) == SdRepositoryAdapterStatus::Pending);
        assert(kicks.service().count() == item);  // Pending is never reported as saved.
        g_result = {};
        g_result.outcome.status = SdRepositoryRequestStatus::Ok;
        g_result.outcome.batch.revisions[0] = 1U;
        assert(kicks.poll() == SdRepositoryAdapterStatus::Ok);
    }
    assert(kicks.service().count() == KickService::kCapacity);
    assert(kicks.begin_upsert(kick) == SdRepositoryAdapterStatus::CapacityReached);
    kick = *kicks.service().at(0U);
    kick.count = 13U;
    assert(kicks.begin_upsert(kick) == SdRepositoryAdapterStatus::Pending);
    g_result.outcome.status = SdRepositoryRequestStatus::JournalError;
    assert(kicks.poll() == SdRepositoryAdapterStatus::IoError);
    assert(kicks.service().find(kick.id)->count == 12U);
    assert(kicks.begin_upsert(kick) == SdRepositoryAdapterStatus::Pending);
    g_result.outcome.status = SdRepositoryRequestStatus::Ok;
    g_result.outcome.batch.revisions[0] = 2U;
    assert(kicks.poll() == SdRepositoryAdapterStatus::Ok);
    assert(kicks.service().find(kick.id)->count == 13U);

    // Load the complete yearly collection through eight-entry pages, including
    // a deleted high ID. No record or numeric-ID floor may be silently lost.
    pregnancy_repository::WeightRepositoryStore yearly;
    assert(yearly.begin_load() == SdRepositoryAdapterStatus::Pending);
    unsigned loaded_reads = 0U, loaded_pages = 0U;
    auto loading = SdRepositoryAdapterStatus::Pending;
    int32_t first_day = 0;
    assert(pregnancy_date_to_day_index({2024U, 1U, 1U}, first_day));
    while (loading == SdRepositoryAdapterStatus::Pending) {
        g_result = {};
        g_result.outcome.status = SdRepositoryRequestStatus::Ok;
        if (g_operation == SdRepositoryRequestOperation::List) {
            ++loaded_pages;
            auto &page = g_result.outcome.page;
            const size_t start = g_page_request.cursor;
            page.count = 367U - start < 8U ? 367U - start : 8U;
            for (size_t entry = 0U; entry < page.count; ++entry) {
                const auto id = start + entry == 366U ? 1000U : start + entry + 1U;
                RepositoryKey key = {};
                assert(pregnancy_repository::weight_key(id, key));
                std::strcpy(page.entries[entry].record_id, key.record_id);
                page.entries[entry].revision = 2U;
                page.entries[entry].tombstone = id == 1000U;
            }
            page.has_more = start + page.count < 367U;
            page.next_cursor = page.has_more ? start + page.count : 0U;
        } else if (g_read_key.repository == RepositoryKind::Preference) {
            g_result.outcome.status = SdRepositoryRequestStatus::NotFound;
        } else {
            ++loaded_reads;
            uint32_t id = 0U;
            assert(pregnancy_repository::numeric_record_id(g_read_key.record_id, id));
            WeightRecord record = {};
            record.id = id;
            assert(pregnancy_date_from_day_index(first_day + id - 1U, record.date));
            record.weight_tenths_kg = 600U;
            pregnancy_repository::EncodedPayload payload = {};
            assert(pregnancy_repository::encode_weight(record, payload) == pregnancy_repository::RecordCodecStatus::Ok);
            auto &result = g_result.outcome.record;
            result.revision = 2U;
            result.payload_size = payload.size;
            std::memcpy(result.payload.data(), payload.bytes.data(), payload.size);
        }
        loading = yearly.poll();
    }
    assert(loading == SdRepositoryAdapterStatus::Ok && yearly.ready());
    assert(yearly.service().count() == 366U && yearly.service().next_id() == 1001U);
    assert(loaded_reads == 366U && loaded_pages == 46U);

    FakeLegacySource legacy;
    legacy.migration_record.key = reminder_key;
    legacy.migration_record.payload.size = 3U;
    legacy.migration_record.payload.bytes[0] = 7U;
    legacy.migration_record.payload.bytes[1] = 8U;
    legacy.migration_record.payload.bytes[2] = 9U;
    LegacyDataMigration migration(legacy);
    assert(migration.begin_import() == LegacyMigrationStatus::Pending);
    assert(migration.report().legacy_kicks_preserved);
    assert(migration.poll() == LegacyMigrationStatus::Pending);
    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::NotFound;
    assert(migration.poll() == LegacyMigrationStatus::Pending);
    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::Ok;
    g_result.outcome.batch.revisions[0] = 1U;
    assert(migration.poll() == LegacyMigrationStatus::Pending);
    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::Ok;
    g_result.outcome.record.payload_size = 3U;
    g_result.outcome.record.payload[0] = 7U;
    g_result.outcome.record.payload[1] = 8U;
    g_result.outcome.record.payload[2] = 9U;
    assert(migration.poll() == LegacyMigrationStatus::Pending);
    assert(migration.poll() == LegacyMigrationStatus::Pending);
    assert(migration.poll() == LegacyMigrationStatus::Pending);
    assert(migration.poll() == LegacyMigrationStatus::CompleteWithConflicts);
    assert(migration.report().records_imported == 1U);
    assert(migration.report().records_corrupt == 1U);
    assert(migration.clear_old_copies() ==
           LegacyMigrationStatus::ClearNotAllowed);
    assert(!legacy.cleared);

    legacy.slots = 1U;
    LegacyDataMigration repeated(legacy);
    assert(repeated.begin_import() == LegacyMigrationStatus::Pending);
    assert(repeated.poll() == LegacyMigrationStatus::Pending);
    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::Ok;
    g_result.outcome.record.payload_size = 3U;
    g_result.outcome.record.payload[0] = 7U;
    g_result.outcome.record.payload[1] = 8U;
    g_result.outcome.record.payload[2] = 9U;
    assert(repeated.poll() == LegacyMigrationStatus::Pending);
    assert(repeated.poll() == LegacyMigrationStatus::Complete);
    assert(repeated.report().records_already_present == 1U);
    assert(repeated.clear_old_copies() == LegacyMigrationStatus::Cleared);
    assert(legacy.cleared);
    SdRepositoryAdapter bound;
    assert(bound.begin_read(key) == SdRepositoryAdapterStatus::Pending);
    ++g_status.mount_generation;
    g_result = {};
    g_result.outcome.status = SdRepositoryRequestStatus::Ok;
    assert(bound.poll() == SdRepositoryAdapterStatus::StaleGeneration);
    assert(bound.begin_upsert(key, 0U, payload.data(), payload.size()) == SdRepositoryAdapterStatus::StaleGeneration);
    bound.reset();
    assert(bound.begin_read(key) == SdRepositoryAdapterStatus::Pending);
    assert(bound.poll() == SdRepositoryAdapterStatus::Ok);
    FakeLegacySource changed_card_source;
    changed_card_source.slots = 0U;
    LegacyDataMigration changed_card(changed_card_source);
    assert(changed_card.begin_import() == LegacyMigrationStatus::Pending);
    assert(changed_card.poll() == LegacyMigrationStatus::Complete);
    ++g_status.mount_generation;
    assert(changed_card.clear_old_copies() == LegacyMigrationStatus::ClearNotAllowed);
    assert(!changed_card_source.cleared);
    return 0;
}
