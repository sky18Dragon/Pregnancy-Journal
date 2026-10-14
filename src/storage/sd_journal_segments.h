#pragma once

#include <cstddef>
#include <cstdint>

#include "storage_contract.h"

namespace sticky_storage {

constexpr uint32_t kSdJournalFirstSegmentNumber = 1U;
constexpr uint32_t kSdJournalLastSegmentNumber = 99999U;
constexpr uint64_t kSdJournalSegmentMaxBytes = 256ULL * 1024ULL;
constexpr uint32_t kSdJournalCheckpointInterval = 128U;

enum class SdJournalSegmentPlanStatus : uint8_t {
    AppendCurrent = 0,
    Rollover,
    InvalidArgument,
    SegmentLimitReached,
};

struct SdJournalSegmentPlan {
    SdJournalSegmentPlanStatus status =
        SdJournalSegmentPlanStatus::InvalidArgument;
    uint32_t segment_number = 0U;
    char file_name[13U] = {};
    char path[kMaxPathBytes] = {};
};

bool sd_journal_segment_name(uint32_t segment_number, char *output,
                             size_t output_capacity);
bool sd_journal_segment_path(uint32_t segment_number, char *output,
                             size_t output_capacity);
SdJournalSegmentPlan sd_journal_plan_append(uint32_t current_segment_number,
                                            uint64_t current_segment_bytes,
                                            size_t encoded_record_bytes);
constexpr bool sd_journal_checkpoint_due(uint32_t records_since_checkpoint)
{
    return records_since_checkpoint >= kSdJournalCheckpointInterval;
}

}  // namespace sticky_storage
