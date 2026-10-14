#include "sd_journal_completion.h"

namespace sticky_storage {

void SdJournalCompletionPool::release(Entry &entry)
{
    entry.id.store(0U, std::memory_order_relaxed);
    entry.request_mount_generation = 0U;
    entry.observed_mount_generation = 0U;
    entry.outcome = {};
    entry.state.store(static_cast<uint8_t>(State::Free),
                      std::memory_order_release);
}

bool SdJournalCompletionPool::reserve(
    uint32_t request_id, uint32_t request_mount_generation,
    SdJournalCompletionToken &token)
{
    token = {};
    if (request_id == 0U || request_mount_generation == 0U) {
        return false;
    }
    for (size_t index = 0U; index < entries_.size(); ++index) {
        Entry &entry = entries_[index];
        uint8_t expected = static_cast<uint8_t>(State::Free);
        if (!entry.state.compare_exchange_strong(
                expected, static_cast<uint8_t>(State::Reserving),
                std::memory_order_acq_rel)) {
            continue;
        }
        entry.id.store(request_id, std::memory_order_relaxed);
        entry.request_mount_generation = request_mount_generation;
        entry.observed_mount_generation = 0U;
        entry.outcome = {};
        entry.state.store(static_cast<uint8_t>(State::Pending),
                          std::memory_order_release);
        token.id = request_id;
        token.slot = static_cast<uint8_t>(index);
        return true;
    }
    return false;
}

bool SdJournalCompletionPool::cancel(const SdJournalCompletionToken &token)
{
    if (token.id == 0U || token.slot >= entries_.size()) {
        return false;
    }
    Entry &entry = entries_[token.slot];
    if (entry.id.load(std::memory_order_acquire) != token.id) {
        return false;
    }
    uint8_t expected = static_cast<uint8_t>(State::Pending);
    if (!entry.state.compare_exchange_strong(
            expected, static_cast<uint8_t>(State::Collecting),
            std::memory_order_acq_rel)) {
        return false;
    }
    release(entry);
    return true;
}

bool SdJournalCompletionPool::complete(
    const SdJournalCompletionToken &token, uint32_t observed_mount_generation,
    const SdJournalRequestResult &outcome)
{
    if (token.id == 0U || token.slot >= entries_.size()) {
        return false;
    }
    Entry &entry = entries_[token.slot];
    if (entry.id.load(std::memory_order_acquire) != token.id) {
        return false;
    }
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

SdJournalCompletionCollectStatus SdJournalCompletionPool::collect(
    const SdJournalCompletionToken &token, SdJournalCompletionRecord &record)
{
    record = {};
    if (token.id == 0U || token.slot >= entries_.size()) {
        return SdJournalCompletionCollectStatus::InvalidToken;
    }
    Entry &entry = entries_[token.slot];
    const State state =
        static_cast<State>(entry.state.load(std::memory_order_acquire));
    if (entry.id.load(std::memory_order_relaxed) != token.id) {
        return SdJournalCompletionCollectStatus::InvalidToken;
    }
    if (state == State::Pending || state == State::Reserving ||
        state == State::Completing) {
        return SdJournalCompletionCollectStatus::Pending;
    }
    if (state != State::Completed) {
        return SdJournalCompletionCollectStatus::InvalidState;
    }
    uint8_t expected = static_cast<uint8_t>(State::Completed);
    if (!entry.state.compare_exchange_strong(
            expected, static_cast<uint8_t>(State::Collecting),
            std::memory_order_acq_rel)) {
        return SdJournalCompletionCollectStatus::InvalidState;
    }
    record.request_id = entry.id.load(std::memory_order_relaxed);
    record.request_mount_generation = entry.request_mount_generation;
    record.observed_mount_generation = entry.observed_mount_generation;
    record.outcome = entry.outcome;
    release(entry);
    return SdJournalCompletionCollectStatus::Ok;
}

}  // namespace sticky_storage
