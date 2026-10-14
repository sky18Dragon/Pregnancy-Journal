#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "storage_repository.h"

namespace sticky_storage {

constexpr size_t kFakeRepositoryCapacity = 32U;

class FakeStorageRepository final : public StorageRepository {
  public:
    RepositoryStatus read(const RepositoryKey &key,
                          RepositoryRecord &record) const override;
    RepositoryStatus list(const RepositoryPageRequest &request,
                          RepositoryPage &page) const override;
    RepositoryStatus commit(const RepositoryMutation &mutation,
                            uint64_t transaction_id,
                            RepositoryBatchResult &result) override;
    RepositoryStatus erase(const RepositoryKey &key,
                           uint32_t expected_revision,
                           uint64_t transaction_id,
                           RepositoryBatchResult &result) override;
    RepositoryStatus commit_batch(
        const RepositoryBatch &batch,
        RepositoryBatchResult &result) override;

    uint64_t last_transaction_id() const { return last_transaction_id_; }
    size_t stored_count() const { return stored_count_; }

  private:
    struct Slot {
        bool occupied = false;
        RepositoryRecord record = {};
    };

    static bool same_key(const RepositoryKey &left,
                         const RepositoryKey &right);
    static uint32_t batch_fingerprint(const RepositoryBatch &batch);
    int find_slot(const RepositoryKey &key) const;
    int find_free_slot() const;

    std::array<Slot, kFakeRepositoryCapacity> slots_ = {};
    size_t stored_count_ = 0U;
    uint64_t last_transaction_id_ = 0U;
    uint32_t last_transaction_fingerprint_ = 0U;
    RepositoryBatchResult last_result_ = {};
};

}  // namespace sticky_storage
