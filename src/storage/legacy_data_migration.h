#pragma once

#include <cstdint>

#include "domain_repository_records.h"
#include "sd_repository_adapter.h"

namespace sticky_storage {

enum class LegacyMigrationReadStatus : uint8_t {
    Record = 0,
    Missing,
    Corrupt,
    IoError,
};

struct LegacyMigrationRecord {
    RepositoryKey key = {};
    pregnancy_repository::EncodedPayload payload = {};
};

class LegacyMigrationSource {
  public:
    virtual ~LegacyMigrationSource() = default;
    virtual uint32_t slot_count() const = 0;
    virtual LegacyMigrationReadStatus read(uint32_t slot,
                                           LegacyMigrationRecord &record) = 0;
    virtual bool legacy_kicks_present() const = 0;
    // Clears only namespaces copied by this migration. The legacy kicks
    // namespace is cleaned separately; deferred domains and unrelated SDK namespaces remain intact.
    virtual bool clear_migrated_namespaces() = 0;
};

struct LegacyMigrationReport {
    uint32_t slots_examined = 0U;
    uint32_t records_valid = 0U;
    uint32_t records_imported = 0U;
    uint32_t records_already_present = 0U;
    uint32_t records_conflicted = 0U;
    uint32_t records_corrupt = 0U;
    bool legacy_kicks_preserved = false;
    bool readback_complete = false;
    bool old_copies_cleared = false;
};

enum class LegacyMigrationStatus : uint8_t {
    Idle = 0,
    Pending,
    Complete,
    CompleteWithConflicts,
    SourceIoError,
    StorageError,
    ClearNotAllowed,
    ClearFailed,
    Cleared,
};

// One-record-at-a-time migration. Existing identical SD payloads are counted
// as already present; different payloads are conflicts and never overwritten.
// Each new commit is read back before it contributes to records_imported.
class LegacyDataMigration {
  public:
    explicit LegacyDataMigration(LegacyMigrationSource &source)
        : source_(source) {}

    LegacyMigrationStatus begin_import();
    LegacyMigrationStatus poll();
    LegacyMigrationStatus clear_old_copies();

    bool pending() const { return status_ == LegacyMigrationStatus::Pending; }
    LegacyMigrationStatus status() const { return status_; }
    const LegacyMigrationReport &report() const { return report_; }
    SdRepositoryAdapterStatus storage_status() const { return storage_status_; }

  private:
    enum class Step : uint8_t { None = 0, Scan, Check, Commit, Verify };

    LegacyMigrationStatus scan_one();
    bool same_payload() const;
    LegacyMigrationStatus storage_failed(SdRepositoryAdapterStatus status);

    LegacyMigrationSource &source_;
    SdRepositoryAdapter adapter_ = {};
    LegacyMigrationRecord record_ = {};
    uint32_t cursor_ = 0U;
    uint32_t mount_generation_ = 0U;
    char dataset_id_[kDatasetIdCharacters + 1U] = {};
    Step step_ = Step::None;
    LegacyMigrationStatus status_ = LegacyMigrationStatus::Idle;
    SdRepositoryAdapterStatus storage_status_ =
        SdRepositoryAdapterStatus::Idle;
    LegacyMigrationReport report_ = {};
};

}  // namespace sticky_storage
