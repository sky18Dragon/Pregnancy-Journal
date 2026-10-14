#pragma once

#include <cstdint>

class Canvas;

namespace sticky_storage {

enum class CheckpointCommitTestPhase : uint8_t {
    Waiting = 0,
    ProbingJournal,
    ProbingIndex,
    Ready,
    Confirm,
    Appending,
    Passed,
    Failed,
};

enum class CheckpointCommitTestAction : uint8_t {
    None = 0,
    Continue,
    Cancel,
    Commit,
};

struct CheckpointCommitTestView {
    CheckpointCommitTestPhase phase = CheckpointCommitTestPhase::Waiting;
    uint32_t journal_records = 0U;
    uint64_t last_transaction_id = 0U;
    uint64_t index_bytes = 0U;
    uint64_t target_transaction_id = 37U;
    uint8_t result_status = 0U;
};

void checkpoint_commit_test_page_render(
    Canvas &canvas, const CheckpointCommitTestView &view);
CheckpointCommitTestAction checkpoint_commit_test_page_action_at(
    const CheckpointCommitTestView &view, int width, int height,
    int x, int y);

}  // namespace sticky_storage
