#include "journal_power_test_pages.h"

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

void draw_body(Canvas &canvas, int y, const char *text, FontWeight weight)
{
    ui_draw_wrapped_text_pixels(
        canvas, 24, y, canvas.width() - 48, text, ui_tokens::kBody,
        GrayLevel::Black, portrait(canvas.width(), canvas.height()) ? 6 : 3,
        ui_tokens::kBodyLineGap, weight);
}

void draw_probe(Canvas &canvas, const JournalPowerTestView &view)
{
    char line[128U] = {};
    std::snprintf(line, sizeof(line),
                  "RECORDS: %u   LAST TX: %llu",
                  static_cast<unsigned>(view.record_count),
                  static_cast<unsigned long long>(
                      view.last_transaction_id));
    canvas.draw_text_pixels(24, portrait(canvas.width(), canvas.height())
                                    ? 152
                                    : 132,
                            line, ui_tokens::kSectionHeading,
                            GrayLevel::Black, FontWeight::Medium);
    std::snprintf(line, sizeof(line), "FILE: %llu B   VALID: %llu B",
                  static_cast<unsigned long long>(view.file_bytes),
                  static_cast<unsigned long long>(view.valid_bytes));
    canvas.draw_text_pixels(24, portrait(canvas.width(), canvas.height())
                                    ? 194
                                    : 168,
                            line, ui_tokens::kBody, GrayLevel::Black,
                            FontWeight::Regular);
}

}  // namespace

void journal_power_test_page_render(Canvas &canvas,
                                    const JournalPowerTestView &view)
{
    prototype_ui::begin_page(canvas);
    const bool is_portrait = portrait(canvas.width(), canvas.height());
    prototype_ui::draw_header(
        canvas,
        view.partial_mode ? "JOURNAL PARTIAL TEST" : "JOURNAL POWER TEST",
        is_portrait ? "S1 CONTROLLED CARD TEST"
                    : "S1 CONTROLLED POWER-LOSS RECOVERY TEST");
    canvas.draw_line(24, is_portrait ? 112 : 98, canvas.width() - 24,
                     is_portrait ? 112 : 98, GrayLevel::Black);

    if (view.phase == JournalPowerTestPhase::Waiting ||
        view.phase == JournalPowerTestPhase::Probing) {
        canvas.draw_text_pixels(
            24, is_portrait ? 152 : 132,
            view.phase == JournalPowerTestPhase::Waiting
                ? "WAITING FOR A VALID DATASET"
                : "READ-ONLY JOURNAL PROBE",
            ui_tokens::kSectionHeading, GrayLevel::Black,
            FontWeight::Medium);
        draw_body(canvas, is_portrait ? 220 : 202,
                  "NO WRITE STARTS AUTOMATICALLY. KEEP THE TEST CARD "
                  "INSERTED.",
                  FontWeight::Regular);
        return;
    }

    draw_probe(canvas, view);
    if (view.phase == JournalPowerTestPhase::Ready) {
        draw_body(canvas, is_portrait ? 260 : 214,
                  view.partial_mode
                      ? "THE TEST SYNCS 172 OF 344 BYTES FOR TX 8. THE "
                        "7-RECORD PREFIX STAYS VALID BUT FUTURE WRITES STOP."
                      : "THE TEST FIRST CREATES ONE VERIFIED OBJECT, THEN "
                        "APPENDS UP TO 7 MAXIMUM-SIZE JOURNAL TRANSACTIONS.",
                  FontWeight::Regular);
        draw_body(canvas, is_portrait ? 430 : 292,
                  "USE ONLY THIS SPARE TEST CARD.", FontWeight::Medium);
        prototype_ui::draw_button(
            canvas, primary_button(canvas.width(), canvas.height()),
            "REVIEW POWER TEST", true, 2);
        return;
    }
    if (view.phase == JournalPowerTestPhase::Confirm) {
        char target[80U] = {};
        std::snprintf(target, sizeof(target), "FIRST TARGET TX: %llu",
                      static_cast<unsigned long long>(
                          view.target_transaction_id));
        draw_body(canvas, is_portrait ? 260 : 214, target,
                  FontWeight::Medium);
        draw_body(canvas, is_portrait ? 336 : 258,
                  view.partial_mode
                      ? "THIS PERMANENTLY LEAVES A TRUNCATED TAIL ON THIS "
                        "TEST DATASET. THE VALID PREFIX IS NOT MODIFIED."
                      : "PREPARE WRITES PWRTEST.BIN ONLY. DO NOT REMOVE "
                        "POWER UNTIL THE NEXT SCREEN SAYS START.",
                  FontWeight::Regular);
        prototype_ui::draw_button(
            canvas, split_button(0U, canvas.width(), canvas.height()),
            "CANCEL", false, 2);
        prototype_ui::draw_button(
            canvas, split_button(1U, canvas.width(), canvas.height()),
            view.partial_mode ? "WRITE PARTIAL TAIL" : "PREPARE", true, 2);
        return;
    }
    if (view.phase == JournalPowerTestPhase::Preparing) {
        draw_body(canvas, is_portrait ? 260 : 214,
                  view.partial_mode
                      ? "WRITING, SYNCING AND VERIFYING THE 172-BYTE TAIL. "
                        "KEEP POWER AND THE CARD CONNECTED."
                      : "WRITING AND VERIFYING THE TEST OBJECT. KEEP POWER "
                        "AND THE CARD CONNECTED.",
                  FontWeight::Medium);
        return;
    }
    if (view.phase == JournalPowerTestPhase::AwaitingReset) {
        canvas.draw_text_pixels(24, is_portrait ? 270 : 224,
                                "PARTIAL JOURNAL SYNCED - RESET NOW",
                                ui_tokens::kPageHeading, GrayLevel::Black,
                                FontWeight::Medium);
        draw_body(canvas, is_portrait ? 350 : 290,
                  "USE THE RESET BUTTON. REBOOT MUST KEEP 2408 VALID BYTES, "
                  "7 RECORDS AND BLOCK ALL LATER WRITES.",
                  FontWeight::Regular);
        return;
    }
    if (view.phase == JournalPowerTestPhase::Armed) {
        draw_body(canvas, is_portrait ? 260 : 214,
                  "FIXTURE READY. TAP START; PRESS RESET AS SOON AS THE "
                  "WRITE CUE SOUNDS.",
                  FontWeight::Medium);
        prototype_ui::draw_button(
            canvas, primary_button(canvas.width(), canvas.height()),
            "START WRITE LOOP", true, 2);
        return;
    }
    if (view.phase == JournalPowerTestPhase::Running) {
        canvas.draw_text_pixels(24, is_portrait ? 270 : 224,
                                "CUT POWER / RESET NOW",
                                ui_tokens::kPageHeading, GrayLevel::Black,
                                FontWeight::Medium);
        draw_body(canvas, is_portrait ? 350 : 290,
                  "DO NOT REMOVE THE MICROSD CARD. RESTART WITH THIS TEST "
                  "FIRMWARE TO PROBE THE VALID PREFIX.",
                  FontWeight::Regular);
        return;
    }
    if (view.phase == JournalPowerTestPhase::Completed) {
        char complete[96U] = {};
        std::snprintf(complete, sizeof(complete),
                      "PASS   %u WRITES VERIFIED",
                      static_cast<unsigned>(view.completed_writes));
        canvas.draw_text_pixels(24, is_portrait ? 270 : 224, complete,
                                ui_tokens::kPageHeading, GrayLevel::Black,
                                FontWeight::Medium);
        draw_body(canvas, is_portrait ? 350 : 290,
                  "NO POWER CUT WAS OBSERVED. RESET NOW TO VERIFY THE "
                  "PERSISTED JOURNAL.",
                  FontWeight::Regular);
        return;
    }
    if (view.phase == JournalPowerTestPhase::TailRecovered) {
        canvas.draw_text_pixels(24, is_portrait ? 270 : 224,
                                "PASS - TRUNCATED TAIL ISOLATED",
                                ui_tokens::kPageHeading, GrayLevel::Black,
                                FontWeight::Medium);
        draw_body(canvas, is_portrait ? 350 : 290,
                  "THE VALID PREFIX REMAINS READABLE. WRITES ARE BLOCKED; "
                  "NO AUTOMATIC REPAIR WAS ATTEMPTED.",
                  FontWeight::Regular);
        return;
    }

    char failure[80U] = {};
    std::snprintf(failure, sizeof(failure), "FAIL   STATUS: %u",
                  static_cast<unsigned>(view.result_status));
    canvas.draw_text_pixels(24, is_portrait ? 270 : 224, failure,
                            ui_tokens::kPageHeading, GrayLevel::Black,
                            FontWeight::Medium);
    draw_body(canvas, is_portrait ? 350 : 290,
              "NO AUTOMATIC RETRY OR REPAIR WAS ATTEMPTED. CHECK THE SERIAL "
              "LOG BEFORE CONTINUING.",
              FontWeight::Regular);
}

JournalPowerTestAction journal_power_test_page_action_at(
    const JournalPowerTestView &view, int width, int height, int x, int y)
{
    if (view.phase == JournalPowerTestPhase::Ready &&
        primary_button(width, height).contains(x, y)) {
        return JournalPowerTestAction::Continue;
    }
    if (view.phase == JournalPowerTestPhase::Confirm) {
        if (split_button(0U, width, height).contains(x, y)) {
            return JournalPowerTestAction::Cancel;
        }
        if (split_button(1U, width, height).contains(x, y)) {
            return JournalPowerTestAction::Prepare;
        }
    }
    if (view.phase == JournalPowerTestPhase::Armed &&
        primary_button(width, height).contains(x, y)) {
        return JournalPowerTestAction::Start;
    }
    return JournalPowerTestAction::None;
}

}  // namespace sticky_storage
