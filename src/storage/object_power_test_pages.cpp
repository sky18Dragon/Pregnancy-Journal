#include "object_power_test_pages.h"

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

void summary(Canvas &canvas, const ObjectPowerTestView &view)
{
    char line[128U] = {};
    std::snprintf(line, sizeof(line), "JOURNAL: %u   LAST TX: %llu",
                  static_cast<unsigned>(view.journal_records),
                  static_cast<unsigned long long>(view.last_transaction_id));
    canvas.draw_text_pixels(24, portrait(canvas.width(), canvas.height())
                                    ? 152
                                    : 132,
                            line, ui_tokens::kSectionHeading,
                            GrayLevel::Black, FontWeight::Medium);
    std::snprintf(line, sizeof(line), "%s: %llu B",
                  view.partial_mode ? "OBJPWR02.BIN" : "OBJPWR01.BIN",
                  static_cast<unsigned long long>(view.object_bytes));
    canvas.draw_text_pixels(24, portrait(canvas.width(), canvas.height())
                                    ? 194
                                    : 168,
                            line, ui_tokens::kBody, GrayLevel::Black,
                            FontWeight::Regular);
}

}  // namespace

void object_power_test_page_render(Canvas &canvas,
                                   const ObjectPowerTestView &view)
{
    prototype_ui::begin_page(canvas);
    const bool is_portrait = portrait(canvas.width(), canvas.height());
    prototype_ui::draw_header(
        canvas,
        view.partial_mode ? "OBJECT PARTIAL TEST" : "OBJECT POWER TEST",
        "S1 OBJECT-BEFORE-JOURNAL BOUNDARY");
    canvas.draw_line(24, is_portrait ? 112 : 98, canvas.width() - 24,
                     is_portrait ? 112 : 98, GrayLevel::Black);

    if (view.phase == ObjectPowerTestPhase::Waiting ||
        view.phase == ObjectPowerTestPhase::ProbingJournal ||
        view.phase == ObjectPowerTestPhase::ProbingObject) {
        body(canvas, is_portrait ? 170 : 132,
             "READ-ONLY PROBE. NO OBJECT OR JOURNAL WRITE STARTS "
             "AUTOMATICALLY.",
             FontWeight::Medium);
        return;
    }

    summary(canvas, view);
    if (view.phase == ObjectPowerTestPhase::Ready) {
        body(canvas, is_portrait ? 260 : 214,
             view.partial_mode
                 ? "THE TEST SYNCS ONLY THE FIRST 512 BYTES OF A NEW "
                   "1024-BYTE OBJECT. IT NEVER APPENDS A JOURNAL COMMIT."
                 : "THE TEST WRITES ONE 1024-BYTE IMMUTABLE OBJECT BUT "
                   "NEVER APPENDS A JOURNAL COMMIT.",
             FontWeight::Regular);
        prototype_ui::draw_button(
            canvas, primary_button(canvas.width(), canvas.height()),
            view.partial_mode ? "REVIEW PARTIAL TEST"
                              : "REVIEW OBJECT TEST",
            true, 2);
        return;
    }
    if (view.phase == ObjectPowerTestPhase::Confirm) {
        body(canvas, is_portrait ? 260 : 214,
             view.partial_mode
                 ? "USE ONLY THIS TEST DATASET. THE WORKER WILL SYNC A "
                   "CONTROLLED SHORT FILE, THEN ASK FOR RESET."
                 : "USE ONLY THIS TEST DATASET. PRESS RESET WHEN THE WRITE "
                   "CUE SOUNDS. THE OBJECT MUST REMAIN UNCOMMITTED.",
             FontWeight::Medium);
        prototype_ui::draw_button(
            canvas, split_button(0U, canvas.width(), canvas.height()),
            "CANCEL", false, 2);
        prototype_ui::draw_button(
            canvas, split_button(1U, canvas.width(), canvas.height()),
            view.partial_mode ? "START PARTIAL WRITE"
                              : "START OBJECT WRITE",
            true, 2);
        return;
    }
    if (view.phase == ObjectPowerTestPhase::Writing) {
        canvas.draw_text_pixels(24, is_portrait ? 270 : 224,
                                view.partial_mode ? "WRITING 512 BYTES"
                                                  : "RESET NOW",
                                ui_tokens::kPageHeading, GrayLevel::Black,
                                FontWeight::Medium);
        body(canvas, is_portrait ? 350 : 290,
             "DO NOT REMOVE THE CARD. REBOOT PROBES THE OBJECT AND THE "
             "UNCHANGED JOURNAL WITHOUT REPAIR.",
             FontWeight::Regular);
        return;
    }
    if (view.phase == ObjectPowerTestPhase::AwaitingReset) {
        canvas.draw_text_pixels(24, is_portrait ? 270 : 224,
                                "PARTIAL FILE SYNCED - RESET NOW",
                                ui_tokens::kPageHeading, GrayLevel::Black,
                                FontWeight::Medium);
        body(canvas, is_portrait ? 350 : 290,
             "USE THE RESET BUTTON. REBOOT MUST ISOLATE THE 512-BYTE FILE "
             "WITHOUT REPAIR OR JOURNAL CHANGE.",
             FontWeight::Regular);
        return;
    }
    if (view.phase == ObjectPowerTestPhase::CompleteOrphan) {
        canvas.draw_text_pixels(24, is_portrait ? 270 : 224,
                                "PASS - COMPLETE ORPHAN ISOLATED",
                                ui_tokens::kPageHeading, GrayLevel::Black,
                                FontWeight::Medium);
        body(canvas, is_portrait ? 350 : 290,
             "THE OBJECT IS EXACT BUT NO JOURNAL COMMIT REFERENCES IT. "
             "NO AUTOMATIC CLEANUP WAS ATTEMPTED.",
             FontWeight::Regular);
        return;
    }
    if (view.phase == ObjectPowerTestPhase::PartialOrphan) {
        canvas.draw_text_pixels(24, is_portrait ? 270 : 224,
                                "PASS - PARTIAL ORPHAN ISOLATED",
                                ui_tokens::kPageHeading, GrayLevel::Black,
                                FontWeight::Medium);
        body(canvas, is_portrait ? 350 : 290,
             "THE PARTIAL OBJECT IS UNCOMMITTED AND BLOCKED FROM "
             "OVERWRITE. NO AUTOMATIC REPAIR WAS ATTEMPTED.",
             FontWeight::Regular);
        return;
    }

    canvas.draw_text_pixels(24, is_portrait ? 270 : 224, "FAIL",
                            ui_tokens::kPageHeading, GrayLevel::Black,
                            FontWeight::Medium);
    body(canvas, is_portrait ? 350 : 290,
         "CHECK THE SERIAL LOG. NO RETRY, DELETE OR REPAIR WAS ATTEMPTED.",
         FontWeight::Regular);
}

ObjectPowerTestAction object_power_test_page_action_at(
    const ObjectPowerTestView &view, int width, int height, int x, int y)
{
    if (view.phase == ObjectPowerTestPhase::Ready &&
        primary_button(width, height).contains(x, y)) {
        return ObjectPowerTestAction::Continue;
    }
    if (view.phase == ObjectPowerTestPhase::Confirm) {
        if (split_button(0U, width, height).contains(x, y)) {
            return ObjectPowerTestAction::Cancel;
        }
        if (split_button(1U, width, height).contains(x, y)) {
            return ObjectPowerTestAction::Start;
        }
    }
    return ObjectPowerTestAction::None;
}

}  // namespace sticky_storage
