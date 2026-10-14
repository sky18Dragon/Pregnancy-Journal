#include "sd_journal_segments.h"

#include <cstdio>
#include <cstring>

#include "sd_journal.h"
#include "sd_storage_layout.h"

namespace sticky_storage {

bool sd_journal_segment_name(uint32_t segment_number, char *output,
                             size_t output_capacity)
{
    if (segment_number < kSdJournalFirstSegmentNumber ||
        segment_number > kSdJournalLastSegmentNumber || output == nullptr ||
        output_capacity < 13U) {
        return false;
    }
    const int written = std::snprintf(output, output_capacity, "TXN%05u.LOG",
                                      static_cast<unsigned>(segment_number));
    return written == 12 && fat83_name_valid(output);
}

bool sd_journal_segment_path(uint32_t segment_number, char *output,
                             size_t output_capacity)
{
    char name[13U] = {};
    if (!sd_journal_segment_name(segment_number, name, sizeof(name)) ||
        output == nullptr) {
        return false;
    }
    const int written = std::snprintf(output, output_capacity, "%s/%s",
                                      kJournalDirectoryPath, name);
    return written > 0 && static_cast<size_t>(written) < output_capacity &&
           static_cast<size_t>(written) < kMaxPathBytes;
}

SdJournalSegmentPlan sd_journal_plan_append(uint32_t current_segment_number,
                                            uint64_t current_segment_bytes,
                                            size_t encoded_record_bytes)
{
    SdJournalSegmentPlan plan = {};
    if (current_segment_number < kSdJournalFirstSegmentNumber ||
        current_segment_number > kSdJournalLastSegmentNumber ||
        current_segment_bytes > kSdJournalSegmentMaxBytes ||
        encoded_record_bytes < kSdJournalHeaderBytes ||
        encoded_record_bytes > kSdJournalMaxRecordBytes) {
        return plan;
    }

    uint32_t target = current_segment_number;
    if (encoded_record_bytes >
        kSdJournalSegmentMaxBytes - current_segment_bytes) {
        if (current_segment_number == kSdJournalLastSegmentNumber) {
            plan.status = SdJournalSegmentPlanStatus::SegmentLimitReached;
            return plan;
        }
        ++target;
        plan.status = SdJournalSegmentPlanStatus::Rollover;
    } else {
        plan.status = SdJournalSegmentPlanStatus::AppendCurrent;
    }
    plan.segment_number = target;
    if (!sd_journal_segment_name(target, plan.file_name,
                                 sizeof(plan.file_name)) ||
        !sd_journal_segment_path(target, plan.path, sizeof(plan.path))) {
        return {};
    }
    return plan;
}

}  // namespace sticky_storage
