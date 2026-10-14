#pragma once

#include <cstdint>

class Canvas;

namespace sticky_storage {

enum class ManifestCommitTestPhase : uint8_t {
    Waiting = 0,
    Ready,
    Confirm,
    Pending,
    Passed,
    Failed,
};

enum class ManifestCommitTestAction : uint8_t {
    None = 0,
    Continue,
    Cancel,
    WriteNext,
};

struct ManifestCommitTestView {
    ManifestCommitTestPhase phase = ManifestCommitTestPhase::Waiting;
    uint64_t current_generation = 0U;
    uint64_t target_generation = 0U;
    char current_slot = '-';
    char target_slot = '-';
    uint8_t result_status = 0U;
};

void manifest_commit_test_page_render(Canvas &canvas,
                                      const ManifestCommitTestView &view);
ManifestCommitTestAction manifest_commit_test_page_action_at(
    const ManifestCommitTestView &view, int width, int height, int x, int y);

}  // namespace sticky_storage
