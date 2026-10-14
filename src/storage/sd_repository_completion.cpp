#include "sd_repository_completion.h"

namespace sticky_storage {

void SdRepositoryCompletionPool::release(Entry &entry)
{
    entry.id.store(0U, std::memory_order_relaxed);
    entry.request = {};
    entry.observed_mount_generation = 0U;
    entry.outcome = {};
    entry.state.store(static_cast<uint8_t>(State::Free),
                      std::memory_order_release);
}

bool SdRepositoryCompletionPool::reserve(
    uint32_t request_id, const SdRepositoryRequest &request,
    SdRepositoryCompletionToken &token)
{
    Entry *entry = acquire(request_id, token);
    if (entry == nullptr) return false;
    entry->request = request;
    return publish(*entry, token, request.mount_generation != 0U);
}

SdRepositoryCompletionPool::Entry *SdRepositoryCompletionPool::acquire(
    uint32_t request_id, SdRepositoryCompletionToken &token)
{
    token = {};
    if (request_id == 0U) return nullptr;
    for (size_t index = 0U; index < entries_.size(); ++index) {
        Entry &entry = entries_[index];
        uint8_t expected = static_cast<uint8_t>(State::Free);
        if (!entry.state.compare_exchange_strong(
                expected, static_cast<uint8_t>(State::Reserving),
                std::memory_order_acq_rel)) {
            continue;
        }
        entry.id.store(request_id, std::memory_order_relaxed);
        entry.request = {};
        entry.observed_mount_generation = 0U;
        entry.outcome = {};
        token.id = request_id;
        token.slot = static_cast<uint8_t>(index);
        return &entry;
    }
    return nullptr;
}

bool SdRepositoryCompletionPool::publish(
    Entry &entry, const SdRepositoryCompletionToken &token,
    bool request_valid)
{
    if (!request_valid) {
        release(entry);
        return false;
    }
    entry.state.store(static_cast<uint8_t>(State::Pending),
                      std::memory_order_release);
    return token.id != 0U;
}

bool SdRepositoryCompletionPool::reserve_read(
    uint32_t request_id, uint32_t mount_generation, const char *dataset_id,
    const RepositoryKey &key, SdRepositoryCompletionToken &token)
{
    Entry *entry = acquire(request_id, token);
    return entry != nullptr && publish(
        *entry, token, sd_repository_make_read_request(
                           mount_generation, dataset_id, key, entry->request));
}

bool SdRepositoryCompletionPool::reserve_list(
    uint32_t request_id, uint32_t mount_generation, const char *dataset_id,
    const RepositoryPageRequest &page, SdRepositoryCompletionToken &token)
{
    Entry *entry = acquire(request_id, token);
    return entry != nullptr && publish(
        *entry, token, sd_repository_make_list_request(
                           mount_generation, dataset_id, page, entry->request));
}

bool SdRepositoryCompletionPool::reserve_commit(
    uint32_t request_id, uint32_t mount_generation, const char *dataset_id,
    const RepositoryMutation &mutation, uint64_t transaction_id,
    SdRepositoryCompletionToken &token)
{
    Entry *entry = acquire(request_id, token);
    return entry != nullptr && publish(
        *entry, token, sd_repository_make_commit_request(
                           mount_generation, dataset_id, mutation,
                           transaction_id, entry->request));
}

bool SdRepositoryCompletionPool::reserve_erase(
    uint32_t request_id, uint32_t mount_generation, const char *dataset_id,
    const RepositoryKey &key, uint32_t expected_revision,
    uint64_t transaction_id, SdRepositoryCompletionToken &token)
{
    Entry *entry = acquire(request_id, token);
    return entry != nullptr && publish(
        *entry, token, sd_repository_make_erase_request(
                           mount_generation, dataset_id, key,
                           expected_revision, transaction_id,
                           entry->request));
}

bool SdRepositoryCompletionPool::cancel(
    const SdRepositoryCompletionToken &token)
{
    if (token.id == 0U || token.slot >= entries_.size()) return false;
    Entry &entry = entries_[token.slot];
    if (entry.id.load(std::memory_order_acquire) != token.id) return false;
    uint8_t expected = static_cast<uint8_t>(State::Pending);
    if (!entry.state.compare_exchange_strong(
            expected, static_cast<uint8_t>(State::Collecting),
            std::memory_order_acq_rel)) {
        return false;
    }
    release(entry);
    return true;
}

bool SdRepositoryCompletionPool::pending_request(
    const SdRepositoryCompletionToken &token,
    const SdRepositoryRequest *&request) const
{
    request = nullptr;
    if (token.id == 0U || token.slot >= entries_.size()) return false;
    const Entry &entry = entries_[token.slot];
    if (entry.id.load(std::memory_order_acquire) != token.id ||
        static_cast<State>(entry.state.load(std::memory_order_acquire)) !=
            State::Pending) {
        return false;
    }
    request = &entry.request;
    return true;
}

bool SdRepositoryCompletionPool::complete(
    const SdRepositoryCompletionToken &token,
    uint32_t observed_mount_generation,
    const SdRepositoryRequestResult &outcome)
{
    if (token.id == 0U || token.slot >= entries_.size()) return false;
    Entry &entry = entries_[token.slot];
    if (entry.id.load(std::memory_order_acquire) != token.id) return false;
    uint8_t expected = static_cast<uint8_t>(State::Pending);
    if (!entry.state.compare_exchange_strong(
            expected, static_cast<uint8_t>(State::Completing),
            std::memory_order_acq_rel)) {
        return false;
    }
    entry.observed_mount_generation = observed_mount_generation;
    entry.outcome = outcome;
    entry.state.store(static_cast<uint8_t>(State::Completed),
                      std::memory_order_release);
    return true;
}

SdRepositoryCompletionCollectStatus SdRepositoryCompletionPool::collect(
    const SdRepositoryCompletionToken &token,
    SdRepositoryCompletionRecord &record)
{
    record = {};
    return collect(token, record.request_id,
                   record.request_mount_generation,
                   record.observed_mount_generation, record.outcome);
}

SdRepositoryCompletionCollectStatus SdRepositoryCompletionPool::collect(
    const SdRepositoryCompletionToken &token, uint32_t &request_id,
    uint32_t &request_mount_generation,
    uint32_t &observed_mount_generation,
    SdRepositoryRequestResult &outcome)
{
    request_id = 0U;
    request_mount_generation = 0U;
    observed_mount_generation = 0U;
    outcome = {};
    if (token.id == 0U || token.slot >= entries_.size()) {
        return SdRepositoryCompletionCollectStatus::InvalidToken;
    }
    Entry &entry = entries_[token.slot];
    const State state =
        static_cast<State>(entry.state.load(std::memory_order_acquire));
    if (entry.id.load(std::memory_order_relaxed) != token.id) {
        return SdRepositoryCompletionCollectStatus::InvalidToken;
    }
    if (state == State::Pending || state == State::Reserving ||
        state == State::Completing) {
        return SdRepositoryCompletionCollectStatus::Pending;
    }
    if (state != State::Completed) {
        return SdRepositoryCompletionCollectStatus::InvalidState;
    }
    uint8_t expected = static_cast<uint8_t>(State::Completed);
    if (!entry.state.compare_exchange_strong(
            expected, static_cast<uint8_t>(State::Collecting),
            std::memory_order_acq_rel)) {
        return SdRepositoryCompletionCollectStatus::InvalidState;
    }
    request_id = entry.id.load(std::memory_order_relaxed);
    request_mount_generation = entry.request.mount_generation;
    observed_mount_generation = entry.observed_mount_generation;
    outcome = entry.outcome;
    release(entry);
    return SdRepositoryCompletionCollectStatus::Ok;
}

}  // namespace sticky_storage
