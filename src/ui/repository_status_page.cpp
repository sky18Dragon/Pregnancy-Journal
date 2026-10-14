#include "repository_status_page.h"
#include "text_layout.h"
#include "ui_language.h"
#include "ui_tokens.h"

namespace repository_status_ui {

prototype_ui::Rect retry_rect(const Canvas &canvas)
{
    return canvas.height() > canvas.width()
               ? prototype_ui::Rect{24, 720, 432, ui_tokens::kButtonHeight}
               : prototype_ui::Rect{24, 400, 752, ui_tokens::kButtonHeight};
}

const char *message(sticky_storage::SdRepositoryAdapterStatus status)
{
    using sticky_storage::SdRepositoryAdapterStatus;
    switch (status) {
    case SdRepositoryAdapterStatus::CardMissing:
        return "MICROSD CARD IS REQUIRED";
    case SdRepositoryAdapterStatus::CardReadOnly:
        return "MICROSD CARD IS READ ONLY";
    case SdRepositoryAdapterStatus::CapacityReached:
        return "RECORD LIMIT REACHED";
    case SdRepositoryAdapterStatus::CardFull:
        return "MICROSD STORAGE IS FULL";
    case SdRepositoryAdapterStatus::DatasetUnavailable:
        return "CREATE OR SELECT A DATASET IN SETTINGS";
    case SdRepositoryAdapterStatus::RecoveryUnavailable:
    case SdRepositoryAdapterStatus::Corrupt:
        return "MICROSD DATA NEEDS REVIEW";
    case SdRepositoryAdapterStatus::StaleGeneration:
        return "MICROSD CARD CHANGED";
    case SdRepositoryAdapterStatus::RevisionConflict:
    case SdRepositoryAdapterStatus::TransactionConflict:
        return "MICROSD DATA CHANGED · RELOAD REQUIRED";
    default:
        return "MICROSD STORAGE UNAVAILABLE";
    }
}

void render(Canvas &canvas, const char *title, bool pending, bool saving,
            sticky_storage::SdRepositoryAdapterStatus status)
{
    prototype_ui::begin_page(canvas);
    prototype_ui::draw_header(canvas, title, "MICROSD USER DATA", "BACK");
    const bool portrait = canvas.height() > canvas.width();
    ui_draw_wrapped_text_pixels(canvas, ui_tokens::kContentMargin, portrait ? 226 : 154, canvas.width() - 2 * ui_tokens::kContentMargin,
        ui_text(pending ? (saving ? "SAVING TO MICROSD" : "LOADING FROM MICROSD") : message(status)),
        ui_tokens::kSectionHeading, GrayLevel::Black, 3, ui_tokens::kBodyLineGap, FontWeight::Medium);
    if (pending) return;
    ui_draw_wrapped_text_pixels(canvas, ui_tokens::kContentMargin, portrait ? 350 : 278, canvas.width() - 2 * ui_tokens::kContentMargin,
        ui_text("CHECK THE CARD, THEN RELOAD."), ui_tokens::kBody, GrayLevel::Black,
        2, ui_tokens::kBodyLineGap, FontWeight::Regular);
    prototype_ui::draw_button(canvas, retry_rect(canvas), saving ? "RETRY" : "RELOAD FROM MICROSD", true, 2);

}

}  // namespace repository_status_ui
