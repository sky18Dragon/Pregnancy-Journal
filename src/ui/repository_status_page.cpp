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

bool can_initialize(const sticky_storage::SdStorageServiceStatus &card)
{
    return card.running && card.accepting_manifest_requests &&
           card.card_state == sticky_storage::SdCardState::ReadyReadWrite &&
           card.mount_generation != 0U && card.manifest_known &&
           card.manifest_state == sticky_storage::SdManifestProbeState::NotInitialized &&
           !card.manifest_selected;
}

State make_state(bool pending, bool saving,
                 sticky_storage::SdRepositoryAdapterStatus status,
                 const sticky_storage::SdStorageServiceStatus &card)
{
    const bool setup = !pending && !saving &&
        status == sticky_storage::SdRepositoryAdapterStatus::DatasetUnavailable &&
        can_initialize(card);
    // A read-only or damaged uninitialized card can also arrive as
    // DatasetUnavailable. Explain that blocker rather than suggesting creation.
    if (!pending && status == sticky_storage::SdRepositoryAdapterStatus::DatasetUnavailable) {
        using sticky_storage::SdCardState;
        using sticky_storage::SdRepositoryAdapterStatus;
        if (card.card_state == SdCardState::Absent || card.card_state == SdCardState::Unmounted)
            status = SdRepositoryAdapterStatus::CardMissing;
        else if (card.card_state == SdCardState::ReadyReadOnly)
            status = SdRepositoryAdapterStatus::CardReadOnly;
        else if (card.card_state == SdCardState::Full)
            status = SdRepositoryAdapterStatus::CardFull;
        else if (card.card_state != SdCardState::ReadyReadWrite || !card.manifest_known)
            status = SdRepositoryAdapterStatus::StorageUnavailable;
        else if (card.manifest_state == sticky_storage::SdManifestProbeState::Corrupt ||
                 card.manifest_state == sticky_storage::SdManifestProbeState::IoError)
            status = SdRepositoryAdapterStatus::Corrupt;
    }
    return {pending, saving, status, setup ? card.mount_generation : 0U};
}

prototype_ui::Rect setup_rect(const Canvas &canvas)
{
    auto rect = retry_rect(canvas);
    rect.y -= ui_tokens::kButtonHeight + ui_tokens::kContentMargin;
    return rect;
}

prototype_ui::Rect target_rect(const Canvas &canvas, Action action)
{
    if (action == Action::Back)
        return prototype_ui::header_action_rect(canvas.width(), canvas.height());
    if (action == Action::Retry) return retry_rect(canvas);
    if (action == Action::Setup) return setup_rect(canvas);
    return {};
}

Action action_at(const Canvas &canvas, const State &state, int x, int y)
{
    if (state.pending) return Action::None;
    if (target_rect(canvas, Action::Back).contains(x, y)) return Action::Back;
    if (state.setup_generation != 0U && setup_rect(canvas).contains(x, y))
        return Action::Setup;
    if (retry_rect(canvas).contains(x, y)) return Action::Retry;
    return Action::None;
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

void render(Canvas &canvas, const char *title, const State &state)
{
    prototype_ui::begin_page(canvas);
    prototype_ui::draw_header(canvas, title, "MICROSD USER DATA", "BACK");
    const bool portrait = canvas.height() > canvas.width();
    const bool setup = state.setup_generation != 0U;
    const char *heading = state.pending
        ? (state.saving ? "SAVING TO MICROSD" : "LOADING FROM MICROSD")
        : (setup ? "SET UP YOUR MEMORY CARD" : message(state.status));
    ui_draw_wrapped_text_pixels(canvas, ui_tokens::kContentMargin,
        portrait ? 226 : 154, canvas.width() - 2 * ui_tokens::kContentMargin,
        ui_text(heading), ui_tokens::kSectionHeading, GrayLevel::Black,
        3, ui_tokens::kBodyLineGap, FontWeight::Medium);
    if (state.pending) return;
    ui_draw_wrapped_text_pixels(canvas, ui_tokens::kContentMargin,
        portrait ? 350 : (setup ? 230 : 278),
        canvas.width() - 2 * ui_tokens::kContentMargin,
        ui_text(setup ? "CREATE APP DATA TO START. OTHER FILES STAY UNCHANGED."
                      : "CHECK THE CARD, THEN RELOAD."),
        ui_tokens::kBody, GrayLevel::Black, portrait ? 4 : 2,
        ui_tokens::kBodyLineGap, FontWeight::Regular);
    if (setup)
        prototype_ui::draw_button(canvas, setup_rect(canvas), "SET UP MEMORY CARD", true, 2);
    prototype_ui::draw_button(canvas, retry_rect(canvas),
        state.saving ? "RETRY" : "RELOAD FROM MICROSD", !setup, 2);
}

}  // namespace repository_status_ui
