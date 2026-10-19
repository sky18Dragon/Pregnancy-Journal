#pragma once

#include "legacy_data_migration.h"

namespace sticky_storage {

// Read-only adapter for the pre-MicroSD application records. It deliberately
// does not use the legacy load helpers because some of those helpers repair or
// rewrite NVS while loading. Migration must leave its source untouched until
// the user explicitly requests deletion after a verified import.
class LegacyNvsSource final : public LegacyMigrationSource {
  public:
    static constexpr uint32_t kSlotCount = 35U;

    uint32_t slot_count() const override { return kSlotCount; }
    LegacyMigrationReadStatus read(uint32_t slot,
                                   LegacyMigrationRecord &record) override;
    bool legacy_kicks_present() const override;
    bool clear_migrated_namespaces() override;
};

class LegacyKickNvsSource final : public LegacyMigrationSource {
  public:
    uint32_t slot_count() const override { return 42U; }
    LegacyMigrationReadStatus read(uint32_t slot, LegacyMigrationRecord &record) override;
    bool legacy_kicks_present() const override { return false; }
    bool clear_migrated_namespaces() override;
};
LegacyKickNvsSource &legacy_kick_nvs_source();

LegacyNvsSource &legacy_nvs_source();

}  // namespace sticky_storage
