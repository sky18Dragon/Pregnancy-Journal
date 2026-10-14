#include "sd_storage_work_gate.h"

namespace sticky_storage {

bool SdStorageWorkGate::kind_valid(SdStorageWorkKind kind)
{
    return kind == SdStorageWorkKind::Record ||
           kind == SdStorageWorkKind::Audio ||
           kind == SdStorageWorkKind::Network;
}

SdStorageWorkStatus SdStorageWorkGate::admit(
    SdStorageWorkKind kind, const SdDatasetBinding &binding,
    uint32_t current_mount_generation, const char *current_dataset_id,
    uint32_t request_id, SdStorageWorkToken &token)
{
    token = {};
    if (!kind_valid(kind) || request_id == 0U) {
        return SdStorageWorkStatus::InvalidArgument;
    }
    if (!accepting_) {
        return SdStorageWorkStatus::AdmissionClosed;
    }
    const SdStoragePolicyStatus binding_status = sd_dataset_binding_validate(
        binding, current_mount_generation, current_dataset_id);
    if (binding_status == SdStoragePolicyStatus::StaleGeneration) {
        return SdStorageWorkStatus::StaleGeneration;
    }
    if (binding_status == SdStoragePolicyStatus::DatasetMismatch) {
        return SdStorageWorkStatus::DatasetMismatch;
    }
    if (binding_status != SdStoragePolicyStatus::Ok) {
        return SdStorageWorkStatus::InvalidArgument;
    }

    for (size_t index = 0U; index < entries_.size(); ++index) {
        Entry &entry = entries_[index];
        if (entry.occupied) {
            continue;
        }
        entry.occupied = true;
        entry.record.id = request_id;
        entry.record.kind = kind;
        entry.record.binding = binding;
        ++pending_;
        token.id = request_id;
        token.slot = static_cast<uint8_t>(index);
        return SdStorageWorkStatus::Ok;
    }
    return SdStorageWorkStatus::CapacityReached;
}

SdStorageWorkStatus SdStorageWorkGate::finish(
    const SdStorageWorkToken &token, uint32_t current_mount_generation,
    const char *current_dataset_id, SdStorageWorkRecord &record)
{
    record = {};
    if (token.id == 0U || token.slot >= entries_.size()) {
        return SdStorageWorkStatus::InvalidToken;
    }
    Entry &entry = entries_[token.slot];
    if (!entry.occupied || entry.record.id != token.id) {
        return SdStorageWorkStatus::InvalidToken;
    }
    record = entry.record;
    entry = {};
    --pending_;

    const SdStoragePolicyStatus binding_status = sd_dataset_binding_validate(
        record.binding, current_mount_generation, current_dataset_id);
    if (binding_status == SdStoragePolicyStatus::StaleGeneration) {
        return SdStorageWorkStatus::StaleGeneration;
    }
    if (binding_status == SdStoragePolicyStatus::DatasetMismatch) {
        return SdStorageWorkStatus::DatasetMismatch;
    }
    return binding_status == SdStoragePolicyStatus::Ok
               ? SdStorageWorkStatus::Ok
               : SdStorageWorkStatus::InvalidArgument;
}

bool SdStorageWorkGate::cancel(const SdStorageWorkToken &token,
                               SdStorageWorkRecord &record)
{
    record = {};
    if (token.id == 0U || token.slot >= entries_.size()) {
        return false;
    }
    Entry &entry = entries_[token.slot];
    if (!entry.occupied || entry.record.id != token.id) {
        return false;
    }
    record = entry.record;
    entry = {};
    --pending_;
    return true;
}

void SdStorageWorkGate::close_admission() { accepting_ = false; }

void SdStorageWorkGate::open_admission() { accepting_ = true; }

bool SdStorageWorkGate::accepting() const { return accepting_; }

size_t SdStorageWorkGate::pending() const { return pending_; }

}  // namespace sticky_storage
