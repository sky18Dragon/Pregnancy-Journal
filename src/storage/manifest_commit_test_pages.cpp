#include "manifest_commit_test_pages.h"

#include <cstdio>

#include "canvas.h"
#include "prototype_ui.h"
#include "text_layout.h"
#include "ui_tokens.h"

namespace sticky_storage {
namespace {

using prototype_ui::Rect;

bool portrait(int width, int height)
{
    return height > width;
}

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
    const bool is_portrait = portrait(canvas.width(), canvas.height());
    ui_draw_wrapped_text_pixels(
        canvas, 24, y, canvas.width() - 48, text, ui_tokens::kBody,
        GrayLevel::Black, is_portrait ? 5 : 3, ui_tokens::kBodyLineGap,
        weight);
}

void draw_generation(Canvas &canvas, const ManifestCommitTestView &view,
                     bool target)
{
    const uint64_t generation = target ? view.target_generation
                                       : view.current_generation;
    const char slot = target ? view.target_slot : view.current_slot;
    if (portrait(canvas.width(), canvas.height())) {
        char generation_text[64] = {};
        char slot_text[32] = {};
        std::snprintf(generation_text, sizeof(generation_text),
                      "%s GENERATION: %llu", target ? "TARGET" : "CURRENT",
                      static_cast<unsigned long long>(generation));
        std::snprintf(slot_text, sizeof(slot_text), "%s SLOT: %c",
                      target ? "TARGET" : "ACTIVE", slot);
        canvas.draw_text_pixels(24, 152, generation_text,
                                ui_tokens::kSectionHeading,
                                GrayLevel::Black, FontWeight::Medium);
        canvas.draw_text_pixels(24, 190, slot_text,
                                ui_tokens::kSectionHeading,
                                GrayLevel::Black, FontWeight::Medium);
        return;
    }

    char value[96] = {};
    std::snprintf(value, sizeof(value), "%s: %llu   SLOT: %c",
                  target ? "TARGET GENERATION" : "CURRENT GENERATION",
                  static_cast<unsigned long long>(generation), slot);
    canvas.draw_text_pixels(24, 132, value, ui_tokens::kSectionHeading,
                            GrayLevel::Black, FontWeight::Medium);
}

}  // namespace

void manifest_commit_test_page_render(Canvas &canvas,
                                      const ManifestCommitTestView &view)
{
    prototype_ui::begin_page(canvas);
    const bool is_portrait = portrait(canvas.width(), canvas.height());
    prototype_ui::draw_header(
        canvas, "MANIFEST COMMIT TEST",
        is_portrait ? "S1 PHYSICAL WRITE TEST"
                    : "S1 PHYSICAL NEXT-GENERATION WRITE VERIFICATION");
    canvas.draw_line(24, is_portrait ? 112 : 98, canvas.width() - 24,
                     is_portrait ? 112 : 98, GrayLevel::Black);

    if (view.phase == ManifestCommitTestPhase::Waiting) {
        canvas.draw_text_pixels(24, is_portrait ? 152 : 132,
                                "WAITING FOR A VALID DATASET",
                                ui_tokens::kSectionHeading, GrayLevel::Black,
                                FontWeight::Medium);
        draw_body(canvas, is_portrait ? 208 : 190,
                  "KEEP THE INITIALIZED CARD INSERTED. NO WRITE STARTS "
                  "AUTOMATICALLY.",
                  FontWeight::Regular);
        return;
    }

    if (view.phase == ManifestCommitTestPhase::Ready) {
        draw_generation(canvas, view, false);
        draw_body(canvas, is_portrait ? 248 : 190,
                  "THIS TEST WRITES THE NEXT MANIFEST COPY ONLY. IT DOES NOT "
                  "FORMAT THE CARD OR CREATE USER RECORDS.",
                  FontWeight::Regular);
        draw_body(canvas, is_portrait ? 430 : 286,
                  "THE CURRENT VALID COPY REMAINS THE RECOVERY POINT UNTIL "
                  "READBACK VERIFICATION PASSES.",
                  FontWeight::Medium);
        prototype_ui::draw_button(
            canvas, primary_button(canvas.width(), canvas.height()),
            "REVIEW WRITE", true, 2);
        return;
    }

    if (view.phase == ManifestCommitTestPhase::Confirm) {
        draw_generation(canvas, view, true);
        draw_body(canvas, is_portrait ? 248 : 190,
                  "WRITE ONLY THE INACTIVE A/B MANIFEST SLOT, THEN FLUSH, "
                  "READ BACK AND VERIFY BOTH COPIES.",
                  FontWeight::Regular);
        draw_body(canvas, is_portrait ? 410 : 286,
                  "KEEP THE CARD INSERTED UNTIL PASS OR FAIL IS SHOWN.",
                  FontWeight::Medium);
        prototype_ui::draw_button(
            canvas, split_button(0U, canvas.width(), canvas.height()),
            "CANCEL", false, 2);
        prototype_ui::draw_button(
            canvas, split_button(1U, canvas.width(), canvas.height()),
            "WRITE NEXT", true, 2);
        return;
    }

    if (view.phase == ManifestCommitTestPhase::Pending) {
        draw_generation(canvas, view, true);
        draw_body(canvas, is_portrait ? 248 : 190,
                  "WRITING AND VERIFYING. KEEP THE CARD INSERTED.",
                  FontWeight::Medium);
        prototype_ui::draw_button(
            canvas, primary_button(canvas.width(), canvas.height()),
            "VERIFYING", false, 2);
        return;
    }

    if (view.phase == ManifestCommitTestPhase::Passed) {
        draw_generation(canvas, view, true);
        canvas.draw_text_pixels(24, is_portrait ? 248 : 202, "PASS",
                                ui_tokens::kPageHeading, GrayLevel::Black,
                                FontWeight::Medium);
        draw_body(canvas, is_portrait ? 316 : 266,
                  "THE NEXT MANIFEST COPY WAS WRITTEN AND READ BACK. REFLASH "
                  "THE NORMAL DEBUG FIRMWARE FOR THE REMOUNT CHECK.",
                  FontWeight::Regular);
        return;
    }

    char failure[80] = {};
    std::snprintf(failure, sizeof(failure), "FAIL   STATUS: %u",
                  static_cast<unsigned>(view.result_status));
    canvas.draw_text_pixels(24, is_portrait ? 152 : 132, failure,
                            ui_tokens::kPageHeading, GrayLevel::Black,
                            FontWeight::Medium);
    draw_body(canvas, is_portrait ? 224 : 202,
              "NO AUTOMATIC RETRY WAS ATTEMPTED. KEEP THE CARD INSERTED AND "
              "CHECK THE SERIAL LOG.",
              FontWeight::Regular);
}

ManifestCommitTestAction manifest_commit_test_page_action_at(
    const ManifestCommitTestView &view, int width, int height, int x, int y)
{
    if (view.phase == ManifestCommitTestPhase::Ready &&
        primary_button(width, height).contains(x, y)) {
        return ManifestCommitTestAction::Continue;
    }
    if (view.phase == ManifestCommitTestPhase::Confirm) {
        if (split_button(0U, width, height).contains(x, y)) {
            return ManifestCommitTestAction::Cancel;
        }
        if (split_button(1U, width, height).contains(x, y)) {
            return ManifestCommitTestAction::WriteNext;
        }
    }
    return ManifestCommitTestAction::None;
}

}  // namespace sticky_storage
