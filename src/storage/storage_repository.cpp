#include "storage_repository.h"

#include <cstring>

namespace sticky_storage {

bool repository_kind_valid(RepositoryKind kind)
{
    return kind >= RepositoryKind::Pregnancy &&
           kind <= RepositoryKind::Kicks;
}

bool repository_key_valid(const RepositoryKey &key)
{
    return repository_kind_valid(key.repository) &&
           sd_dataset_id_valid(key.record_id);
}

bool repository_key_make(RepositoryKind repository, const char *record_id,
                         RepositoryKey &key)
{
    key = {};
    if (!repository_kind_valid(repository) ||
        !sd_dataset_id_valid(record_id)) {
        return false;
    }
    key.repository = repository;
    std::memcpy(key.record_id, record_id, sizeof(key.record_id));
    return true;
}

bool repository_mutation_make_upsert(
    const RepositoryKey &key, uint32_t expected_revision,
    const uint8_t *payload, size_t payload_size,
    RepositoryMutation &mutation)
{
    mutation = {};
    if (!repository_key_valid(key) ||
        (payload == nullptr && payload_size != 0U) ||
        payload_size > mutation.payload.size()) {
        return false;
    }
    mutation.type = RepositoryMutationType::Upsert;
    mutation.key = key;
    mutation.expected_revision = expected_revision;
    mutation.payload_size = payload_size;
    if (payload_size != 0U) {
        std::memcpy(mutation.payload.data(), payload, payload_size);
    }
    return true;
}

bool repository_mutation_make_delete(const RepositoryKey &key,
                                     uint32_t expected_revision,
                                     RepositoryMutation &mutation)
{
    mutation = {};
    if (!repository_key_valid(key) || expected_revision == 0U) {
        return false;
    }
    mutation.type = RepositoryMutationType::Delete;
    mutation.key = key;
    mutation.expected_revision = expected_revision;
    return true;
}

}  // namespace sticky_storage
