#pragma once

#include <cstdint>

class Canvas;

namespace sticky_storage {

enum class IndexPowerTestPhase : uint8_t {
    Waiting = 0,
    ProbingJournal,
    ProbingIndex,
    Ready,
    Confirm,
    Writing,
    AwaitingReset,
    CompleteOrphan,
    PartialOrphan,
    Failed,
};

enum class IndexPowerTestAction : uint8_t {
    None = 0,
    Continue,
    Cancel,
    Start,
};

struct IndexPowerTestView {
    IndexPowerTestPhase phase = IndexPowerTestPhase::Waiting;
    bool partial_mode = false;
    uint32_t journal_records = 0U;
    uint64_t last_transaction_id = 0U;
    uint64_t index_bytes = 0U;
};

void index_power_test_page_render(Canvas &canvas,
                                  const IndexPowerTestView &view);
IndexPowerTestAction index_power_test_page_action_at(
    const IndexPowerTestView &view, int width, int height, int x, int y);

}  // namespace sticky_storage
