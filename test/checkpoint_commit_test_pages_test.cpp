#include <cassert>

#include "checkpoint_commit_test_pages.h"

int main()
{
    using namespace sticky_storage;
    CheckpointCommitTestView view = {};
    view.phase = CheckpointCommitTestPhase::Ready;
    assert(checkpoint_commit_test_page_action_at(
               view, 800, 480, 400, 428) ==
           CheckpointCommitTestAction::Continue);
    assert(checkpoint_commit_test_page_action_at(
               view, 800, 480, 400, 300) ==
           CheckpointCommitTestAction::None);

    view.phase = CheckpointCommitTestPhase::Confirm;
    assert(checkpoint_commit_test_page_action_at(
               view, 800, 480, 180, 428) ==
           CheckpointCommitTestAction::Cancel);
    assert(checkpoint_commit_test_page_action_at(
               view, 800, 480, 600, 428) ==
           CheckpointCommitTestAction::Commit);

    assert(checkpoint_commit_test_page_action_at(
               view, 480, 800, 120, 748) ==
           CheckpointCommitTestAction::Cancel);
    assert(checkpoint_commit_test_page_action_at(
               view, 480, 800, 360, 748) ==
           CheckpointCommitTestAction::Commit);

    view.phase = CheckpointCommitTestPhase::Appending;
    assert(checkpoint_commit_test_page_action_at(
               view, 480, 800, 360, 748) ==
           CheckpointCommitTestAction::None);
    return 0;
}
