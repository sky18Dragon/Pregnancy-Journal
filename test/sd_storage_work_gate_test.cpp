#include <array>
#include <cassert>
#include <cstring>

#include "sd_storage_work_gate.h"

int main()
{
    using namespace sticky_storage;
    constexpr char dataset_a[] = "0123456789abcdef0123456789abcdef";
    constexpr char dataset_b[] = "fedcba9876543210fedcba9876543210";

    SdDatasetBinding binding = {};
    assert(sd_dataset_binding_make(7U, dataset_a, binding));
    SdStorageWorkGate gate;
    SdStorageWorkToken token = {};
    assert(gate.admit(SdStorageWorkKind::Record, binding, 7U, dataset_a,
                      1U, token) == SdStorageWorkStatus::Ok);
    assert(gate.pending() == 1U && gate.accepting());

    gate.close_admission();
    SdStorageWorkToken rejected = {};
    assert(gate.admit(SdStorageWorkKind::Record, binding, 7U, dataset_a, 2U,
                      rejected) == SdStorageWorkStatus::AdmissionClosed);

    SdStorageWorkRecord record = {};
    assert(gate.finish(token, 8U, dataset_a, record) ==
           SdStorageWorkStatus::StaleGeneration);
    assert(record.kind == SdStorageWorkKind::Record);
    assert(record.binding.mount_generation == 7U);
    assert(gate.pending() == 0U);
    assert(gate.finish(token, 8U, dataset_a, record) ==
           SdStorageWorkStatus::InvalidToken);

    gate.open_admission();
    assert(gate.admit(SdStorageWorkKind::Record, binding, 7U, dataset_a, 3U,
                      token) == SdStorageWorkStatus::Ok);
    assert(gate.finish(token, 7U, dataset_b, record) ==
           SdStorageWorkStatus::DatasetMismatch);

    std::array<SdStorageWorkToken, kStorageQueueDepth> tokens = {};
    for (size_t index = 0U; index < tokens.size(); ++index) {
        assert(gate.admit(SdStorageWorkKind::Record, binding, 7U, dataset_a,
                          static_cast<uint32_t>(10U + index),
                          tokens[index]) == SdStorageWorkStatus::Ok);
    }
    assert(gate.admit(SdStorageWorkKind::Record, binding, 7U, dataset_a, 99U,
                      rejected) == SdStorageWorkStatus::CapacityReached);
    assert(gate.cancel(tokens[0], record));
    assert(!gate.cancel(tokens[0], record));
    assert(gate.admit(SdStorageWorkKind::Record, binding, 7U, dataset_a, 100U,
                      rejected) == SdStorageWorkStatus::Ok);

    SdDatasetBinding stale = {};
    assert(sd_dataset_binding_make(6U, dataset_a, stale));
    assert(gate.admit(SdStorageWorkKind::Record, stale, 7U, dataset_a, 101U,
                      token) == SdStorageWorkStatus::StaleGeneration);
    SdDatasetBinding other = {};
    assert(sd_dataset_binding_make(7U, dataset_b, other));
    assert(gate.admit(SdStorageWorkKind::Record, other, 7U, dataset_a, 102U,
                      token) == SdStorageWorkStatus::DatasetMismatch);
    assert(gate.admit(static_cast<SdStorageWorkKind>(99U), binding, 7U,
                      dataset_a, 103U, token) ==
           SdStorageWorkStatus::InvalidArgument);
    return 0;
}
