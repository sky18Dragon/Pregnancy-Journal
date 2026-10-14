#pragma once

#include <cstdint>

class Canvas;

namespace sticky_storage {

enum class JournalPowerTestPhase : uint8_t {
    Waiting = 0,
    Probing,
    Ready,
    Confirm,
    Preparing,
    AwaitingReset,
    Armed,
    Running,
    Completed,
    TailRecovered,
    Failed,
};

enum class JournalPowerTestAction : uint8_t {
    None = 0,
    Continue,
    Cancel,
    Prepare,
    Start,
};

struct JournalPowerTestView {
    JournalPowerTestPhase phase = JournalPowerTestPhase::Waiting;
    bool partial_mode = false;
    uint8_t probe_status = 0U;
    uint8_t result_status = 0U;
    uint32_t record_count = 0U;
    uint64_t file_bytes = 0U;
    uint64_t valid_bytes = 0U;
    uint64_t last_transaction_id = 0U;
    uint64_t target_transaction_id = 0U;
    uint32_t completed_writes = 0U;
};

void journal_power_test_page_render(Canvas &canvas,
                                    const JournalPowerTestView &view);
JournalPowerTestAction journal_power_test_page_action_at(
    const JournalPowerTestView &view, int width, int height, int x, int y);

}  // namespace sticky_storage
