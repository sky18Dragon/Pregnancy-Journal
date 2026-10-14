#include "legacy_data_migration.h"

#include <cstring>

namespace sticky_storage {

LegacyMigrationStatus LegacyDataMigration::begin_import()
{
    if (pending()) return status_;
    const auto card = sd_storage_service_status();
    mount_generation_ = card.mount_generation;
    std::memcpy(dataset_id_, card.selected_manifest.dataset_id, sizeof(dataset_id_));
    adapter_.reset();
    cursor_ = 0U;
    step_ = Step::Scan;
    status_ = LegacyMigrationStatus::Pending;
    storage_status_ = SdRepositoryAdapterStatus::Idle;
    report_ = {};
    report_.legacy_kicks_preserved = source_.legacy_kicks_present();
    return status_;
}

bool LegacyDataMigration::same_payload() const
{
    const RepositoryRecord &stored = adapter_.result().outcome.record;
    return !stored.tombstone && stored.payload_size == record_.payload.size &&
           std::memcmp(stored.payload.data(), record_.payload.bytes.data(),
                       record_.payload.size) == 0;
}

LegacyMigrationStatus LegacyDataMigration::storage_failed(
    SdRepositoryAdapterStatus status)
{
    storage_status_ = status;
    step_ = Step::None;
    status_ = LegacyMigrationStatus::StorageError;
    return status_;
}

LegacyMigrationStatus LegacyDataMigration::scan_one()
{
    if (cursor_ >= source_.slot_count()) {
        report_.readback_complete = true;
        step_ = Step::None;
        status_ = report_.records_conflicted == 0U &&
                          report_.records_corrupt == 0U
                      ? LegacyMigrationStatus::Complete
                      : LegacyMigrationStatus::CompleteWithConflicts;
        return status_;
    }

    record_ = {};
    const LegacyMigrationReadStatus read = source_.read(cursor_++, record_);
    ++report_.slots_examined;
    if (read == LegacyMigrationReadStatus::Missing) return status_;
    if (read == LegacyMigrationReadStatus::Corrupt) {
        ++report_.records_corrupt;
        return status_;
    }
    if (read == LegacyMigrationReadStatus::IoError) {
        step_ = Step::None;
        status_ = LegacyMigrationStatus::SourceIoError;
        return status_;
    }
    if (!repository_key_valid(record_.key) || record_.payload.size == 0U ||
        record_.payload.size > record_.payload.bytes.size()) {
        ++report_.records_corrupt;
        return status_;
    }
    ++report_.records_valid;
    storage_status_ = adapter_.begin_read(record_.key);
    if (storage_status_ != SdRepositoryAdapterStatus::Pending)
        return storage_failed(storage_status_);
    step_ = Step::Check;
    return status_;
}

LegacyMigrationStatus LegacyDataMigration::poll()
{
    if (!pending()) return status_;
    if (step_ == Step::Scan) return scan_one();

    storage_status_ = adapter_.poll();
    if (storage_status_ == SdRepositoryAdapterStatus::Pending) return status_;
    if (step_ == Step::Check) {
        if (storage_status_ == SdRepositoryAdapterStatus::NotFound) {
            storage_status_ = adapter_.begin_upsert(
                record_.key, 0U, record_.payload.bytes.data(),
                record_.payload.size);
            if (storage_status_ != SdRepositoryAdapterStatus::Pending)
                return storage_failed(storage_status_);
            step_ = Step::Commit;
            return status_;
        }
        if (storage_status_ == SdRepositoryAdapterStatus::Tombstoned) {
            ++report_.records_conflicted;
            step_ = Step::Scan;
            return status_;
        }
        if (storage_status_ != SdRepositoryAdapterStatus::Ok)
            return storage_failed(storage_status_);
        if (same_payload()) ++report_.records_already_present;
        else ++report_.records_conflicted;
        step_ = Step::Scan;
        return status_;
    }
    if (step_ == Step::Commit) {
        if (storage_status_ != SdRepositoryAdapterStatus::Ok)
            return storage_failed(storage_status_);
        storage_status_ = adapter_.begin_read(record_.key);
        if (storage_status_ != SdRepositoryAdapterStatus::Pending)
            return storage_failed(storage_status_);
        step_ = Step::Verify;
        return status_;
    }
    if (step_ == Step::Verify) {
        if (storage_status_ != SdRepositoryAdapterStatus::Ok || !same_payload())
            return storage_failed(storage_status_ ==
                                          SdRepositoryAdapterStatus::Ok
                                      ? SdRepositoryAdapterStatus::Corrupt
                                      : storage_status_);
        ++report_.records_imported;
        step_ = Step::Scan;
        return status_;
    }
    return storage_failed(SdRepositoryAdapterStatus::Corrupt);
}

LegacyMigrationStatus LegacyDataMigration::clear_old_copies()
{
    const auto card = sd_storage_service_status();
    if (card.mount_generation != mount_generation_ || !card.manifest_selected ||
        std::strcmp(card.selected_manifest.dataset_id, dataset_id_) != 0 ||
        sd_repository_adapter_preflight(card, false) != SdRepositoryAdapterStatus::Ok ||
        status_ != LegacyMigrationStatus::Complete ||
        !report_.readback_complete || report_.records_conflicted != 0U ||
        report_.records_corrupt != 0U) {
        status_ = LegacyMigrationStatus::ClearNotAllowed;
        return status_;
    }
    if (!source_.clear_migrated_namespaces()) {
        status_ = LegacyMigrationStatus::ClearFailed;
        return status_;
    }
    report_.old_copies_cleared = true;
    status_ = LegacyMigrationStatus::Cleared;
    return status_;
}

}  // namespace sticky_storage
