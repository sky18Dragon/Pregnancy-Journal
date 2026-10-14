#include "checkpoint_commit_test_pages.h"

#include <cstdio>

#include "canvas.h"
#include "prototype_ui.h"
#include "text_layout.h"
#include "ui_tokens.h"

namespace sticky_storage {
namespace {

using prototype_ui::Rect;

bool portrait(int width, int height) { return height > width; }

Rect primary_button(int width, int height)
{
    return portrait(width, height) ? Rect{24, 720, 432, 56}
                                   : Rect{24, 400, 752, 56};
}

Rect split_button(size_t index, int width, int height)
{
    if (portrait(width, height)) {
        return {24 + static_cast<int>(index) * 224, 720, 208, 56};
    }
    return index == 0U ? Rect{24, 400, 368, 56}
                       : Rect{408, 400, 368, 56};
}

void body(Canvas &canvas, int y, const char *text, FontWeight weight)
{
    ui_draw_wrapped_text_pixels(
        canvas, 24, y, canvas.width() - 48, text, ui_tokens::kBody,
        GrayLevel::Black, portrait(canvas.width(), canvas.height()) ? 7 : 3,
        ui_tokens::kBodyLineGap, weight);
}

void summary(Canvas &canvas, const CheckpointCommitTestView &view)
{
    const bool is_portrait = portrait(canvas.width(), canvas.height());
    char line[128U] = {};
    std::snprintf(line, sizeof(line), "JOURNAL: %u   LAST TX: %llu",
                  static_cast<unsigned>(view.journal_records),
                  static_cast<unsigned long long>(view.last_transaction_id));
    canvas.draw_text_pixels(24, is_portrait ? 152 : 132, line,
                            ui_tokens::kSectionHeading, GrayLevel::Black,
                            FontWeight::Medium);
    std::snprintf(line, sizeof(line),
                  "INDEX: %llu B   CHECKPOINT TX: %llu",
                  static_cast<unsigned long long>(view.index_bytes),
                  static_cast<unsigned long long>(
                      view.target_transaction_id));
    canvas.draw_text_pixels(24, is_portrait ? 194 : 168, line,
                            ui_tokens::kBody, GrayLevel::Black,
                            FontWeight::Regular);
}

}  // namespace

void checkpoint_commit_test_page_render(
    Canvas &canvas, const CheckpointCommitTestView &view)
{
    prototype_ui::begin_page(canvas);
    const bool is_portrait = portrait(canvas.width(), canvas.height());
    prototype_ui::draw_header(canvas, "CHECKPOINT COMMIT TEST",
                              "S1 INDEX CHECKPOINT BOUNDARY");
    canvas.draw_line(24, is_portrait ? 112 : 98, canvas.width() - 24,
                     is_portrait ? 112 : 98, GrayLevel::Black);

    if (view.phase == CheckpointCommitTestPhase::Waiting ||
        view.phase == CheckpointCommitTestPhase::ProbingJournal ||
        view.phase == CheckpointCommitTestPhase::ProbingIndex) {
        body(canvas, is_portrait ? 170 : 132,
             "READ-ONLY PROBE. NO CHECKPOINT WRITE STARTS AUTOMATICALLY.",
             FontWeight::Medium);
        return;
    }

    summary(canvas, view);
    if (view.phase == CheckpointCommitTestPhase::Ready) {
        body(canvas, is_portrait ? 260 : 214,
             "THE VERIFIED INDEX COVERS TRANSACTION 36. THIS TEST APPENDS "
             "CHECKPOINT TRANSACTION 37.",
             FontWeight::Regular);
        prototype_ui::draw_button(
            canvas, primary_button(canvas.width(), canvas.height()),
            "REVIEW CHECKPOINT", true, 2);
        return;
    }
    if (view.phase == CheckpointCommitTestPhase::Confirm) {
        body(canvas, is_portrait ? 260 : 214,
             "KEEP THE CARD INSERTED. THE CHECKPOINT BECOMES VALID ONLY "
             "AFTER APPEND, SYNC AND FULL READBACK.",
             FontWeight::Medium);
        prototype_ui::draw_button(
            canvas, split_button(0U, canvas.width(), canvas.height()),
            "CANCEL", false, 2);
        prototype_ui::draw_button(
            canvas, split_button(1U, canvas.width(), canvas.height()),
            "COMMIT CHECKPOINT", true, 2);
        return;
    }
    if (view.phase == CheckpointCommitTestPhase::Appending) {
        body(canvas, is_portrait ? 270 : 224,
             "WRITING AND VERIFYING. DO NOT RESET OR REMOVE THE CARD.",
             FontWeight::Medium);
        prototype_ui::draw_button(
            canvas, primary_button(canvas.width(), canvas.height()),
            "VERIFYING", false, 2);
        return;
    }
    if (view.phase == CheckpointCommitTestPhase::Passed) {
        canvas.draw_text_pixels(24, is_portrait ? 270 : 224,
                                "PASS - CHECKPOINT COMMITTED",
                                ui_tokens::kPageHeading, GrayLevel::Black,
                                FontWeight::Medium);
        body(canvas, is_portrait ? 350 : 290,
             "TRANSACTION 37 REFERENCES THE EXACT TRANSACTION-36 INDEX. "
             "REBOOT TO CONFIRM THE RECOVERY POINT PERSISTS.",
             FontWeight::Regular);
        return;
    }

    char failure[64U] = {};
    std::snprintf(failure, sizeof(failure), "FAIL   STATUS: %u",
                  static_cast<unsigned>(view.result_status));
    canvas.draw_text_pixels(24, is_portrait ? 270 : 224, failure,
                            ui_tokens::kPageHeading, GrayLevel::Black,
                            FontWeight::Medium);
    body(canvas, is_portrait ? 350 : 290,
         "NO AUTOMATIC RETRY OR REPAIR WAS ATTEMPTED. CHECK SERIAL LOG.",
         FontWeight::Regular);
}

CheckpointCommitTestAction checkpoint_commit_test_page_action_at(
    const CheckpointCommitTestView &view, int width, int height,
    int x, int y)
{
    if (view.phase == CheckpointCommitTestPhase::Ready &&
        primary_button(width, height).contains(x, y)) {
        return CheckpointCommitTestAction::Continue;
    }
    if (view.phase == CheckpointCommitTestPhase::Confirm) {
        if (split_button(0U, width, height).contains(x, y)) {
            return CheckpointCommitTestAction::Cancel;
        }
        if (split_button(1U, width, height).contains(x, y)) {
            return CheckpointCommitTestAction::Commit;
        }
    }
    return CheckpointCommitTestAction::None;
}

}  // namespace sticky_storage
