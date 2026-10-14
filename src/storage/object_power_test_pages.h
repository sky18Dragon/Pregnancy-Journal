#pragma once

#include <cstdint>

class Canvas;

namespace sticky_storage {

enum class ObjectPowerTestPhase : uint8_t {
    Waiting = 0,
    ProbingJournal,
    ProbingObject,
    Ready,
    Confirm,
    Writing,
    AwaitingReset,
    CompleteOrphan,
    PartialOrphan,
    Failed,
};

enum class ObjectPowerTestAction : uint8_t {
    None = 0,
    Continue,
    Cancel,
    Start,
};

struct ObjectPowerTestView {
    ObjectPowerTestPhase phase = ObjectPowerTestPhase::Waiting;
    bool partial_mode = false;
    uint32_t journal_records = 0U;
    uint64_t last_transaction_id = 0U;
    uint64_t object_bytes = 0U;
    uint32_t object_crc32 = 0U;
    uint8_t object_status = 0U;
};

void object_power_test_page_render(Canvas &canvas,
                                   const ObjectPowerTestView &view);
ObjectPowerTestAction object_power_test_page_action_at(
    const ObjectPowerTestView &view, int width, int height, int x, int y);

}  // namespace sticky_storage
