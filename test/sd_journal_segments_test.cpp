#include <cassert>
#include <cstring>

#include "sd_journal.h"
#include "sd_journal_segments.h"
#include "sd_storage_layout.h"

int main()
{
    using namespace sticky_storage;
    char name[13U] = {};
    char path[kMaxPathBytes] = {};
    assert(sd_journal_segment_name(1U, name, sizeof(name)));
    assert(std::strcmp(name, "TXN00001.LOG") == 0);
    assert(sd_journal_segment_path(99999U, path, sizeof(path)));
    assert(std::strcmp(path,
                       "/sdcard/PREGJNL/JOURNAL/TXN99999.LOG") == 0);
    assert(!sd_journal_segment_name(0U, name, sizeof(name)));
    assert(!sd_journal_segment_name(100000U, name, sizeof(name)));

    SdJournalSegmentPlan plan = sd_journal_plan_append(
        1U, 0U, kSdJournalHeaderBytes);
    assert(plan.status == SdJournalSegmentPlanStatus::AppendCurrent);
    assert(plan.segment_number == 1U);
    assert(std::strcmp(plan.file_name, kJournalFirstSegmentFile) == 0);
    plan = sd_journal_plan_append(
        1U, kSdJournalSegmentMaxBytes - kSdJournalHeaderBytes,
        kSdJournalHeaderBytes);
    assert(plan.status == SdJournalSegmentPlanStatus::AppendCurrent);
    plan = sd_journal_plan_append(
        1U, kSdJournalSegmentMaxBytes - kSdJournalHeaderBytes + 1U,
        kSdJournalHeaderBytes);
    assert(plan.status == SdJournalSegmentPlanStatus::Rollover);
    assert(plan.segment_number == 2U);
    assert(std::strcmp(plan.file_name, "TXN00002.LOG") == 0);
    plan = sd_journal_plan_append(
        kSdJournalLastSegmentNumber, kSdJournalSegmentMaxBytes,
        kSdJournalHeaderBytes);
    assert(plan.status == SdJournalSegmentPlanStatus::SegmentLimitReached);
    assert(sd_journal_checkpoint_due(kSdJournalCheckpointInterval));
    assert(!sd_journal_checkpoint_due(kSdJournalCheckpointInterval - 1U));
    return 0;
}
