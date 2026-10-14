#include "sticky_sdcard.h"

#include "board_sd_card.h"

namespace {

#if defined(STICKY_SD_INJECT_MOUNT_FAILURE_ONCE) && \
    STICKY_SD_INJECT_MOUNT_FAILURE_ONCE
esp_err_t board_mount_with_injected_failure()
{
    static bool failure_injected = false;
    if (!failure_injected) {
        failure_injected = true;
        return ESP_FAIL;
    }
    return board_sd_card_mount();
}
#endif

#if defined(STICKY_SD_INJECT_UNMOUNT_FAILURE_ONCE) && \
    STICKY_SD_INJECT_UNMOUNT_FAILURE_ONCE
esp_err_t board_unmount_with_injected_failure()
{
    static bool failure_injected = false;
    if (!failure_injected) {
        failure_injected = true;
        return ESP_FAIL;
    }
    return board_sd_card_unmount();
}
#endif

const StickySdCardBoardOps kBoardOps = {
    board_sd_card_present,
#if defined(STICKY_SD_INJECT_MOUNT_FAILURE_ONCE) && \
    STICKY_SD_INJECT_MOUNT_FAILURE_ONCE
    board_mount_with_injected_failure,
#else
    board_sd_card_mount,
#endif
#if defined(STICKY_SD_INJECT_UNMOUNT_FAILURE_ONCE) && \
    STICKY_SD_INJECT_UNMOUNT_FAILURE_ONCE
    board_unmount_with_injected_failure,
#else
    board_sd_card_unmount,
#endif
    board_sd_card_space,
};

}  // namespace

StickySdCard::StickySdCard() : StickySdCard(kBoardOps) {}

StickySdCard::StickySdCard(const StickySdCardBoardOps &board) : board_(board) {}

bool StickySdCard::valid_board() const
{
    return board_.present != nullptr && board_.mount != nullptr &&
           board_.unmount != nullptr && board_.space != nullptr;
}

bool StickySdCard::sample_present()
{
    present_ = board_.present();
    presence_known_ = true;
    return present_;
}

esp_err_t StickySdCard::mount()
{
    if (!valid_board()) {
        return ESP_ERR_INVALID_ARG;
    }

    const bool card_present = sample_present();
    if (!state_.begin_mount(card_present)) {
        return card_present ? ESP_ERR_INVALID_STATE : ESP_ERR_NOT_FOUND;
    }

    const esp_err_t result = board_.mount();
    if (result == ESP_OK) {
#if defined(STICKY_SD_INJECT_READ_ONLY) && STICKY_SD_INJECT_READ_ONLY
        constexpr sticky_storage::SdCardAccess access =
            sticky_storage::SdCardAccess::ReadOnly;
#else
        constexpr sticky_storage::SdCardAccess access =
            sticky_storage::SdCardAccess::ReadWrite;
#endif
        return state_.complete_mount(access)
                   ? ESP_OK
                   : ESP_ERR_INVALID_STATE;
    }

    state_.fail_mount(sample_present());
    return result;
}

StickySdCardPollResult StickySdCard::poll()
{
    StickySdCardPollResult result = {};
    if (!valid_board()) {
        result.error = ESP_ERR_INVALID_ARG;
        return result;
    }

    const bool previous_present = present_;
    const bool was_known = presence_known_;
    const bool card_present = sample_present();
    if (!was_known) {
        if (card_present) {
            result.event = StickySdCardEvent::Inserted;
        }
        return result;
    }
    if (card_present == previous_present) {
        return result;
    }

    if (card_present) {
        result.event = StickySdCardEvent::Inserted;
        return result;
    }

    if (!state_.mounted()) {
        state_.card_removed();
        result.event = StickySdCardEvent::Removed;
        return result;
    }

    // Revoke access before touching the low-level mount. Existing file handles
    // are invalid after physical removal even if IDF cleanup later fails.
    state_.mark_fault();
    result.error = board_.unmount();
    if (result.error == ESP_OK) {
        state_.card_removed();
        result.event = StickySdCardEvent::Removed;
    } else {
        result.event = StickySdCardEvent::RemovalCleanupFailed;
    }
    return result;
}

esp_err_t StickySdCard::begin_eject()
{
    return state_.begin_eject() ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t StickySdCard::finish_eject()
{
    if (state_.state() != sticky_storage::SdCardState::Ejecting ||
        !state_.mounted()) {
        return ESP_ERR_INVALID_STATE;
    }

    const esp_err_t result = board_.unmount();
    if (result == ESP_OK) {
        state_.complete_eject();
    } else {
        state_.fail_eject();
    }
    return result;
}

esp_err_t StickySdCard::recover_removed_card()
{
    if (!valid_board()) {
        return ESP_ERR_INVALID_ARG;
    }
    if (sample_present() || !state_.mounted() ||
        state_.state() != sticky_storage::SdCardState::Fault) {
        return ESP_ERR_INVALID_STATE;
    }

    const esp_err_t result = board_.unmount();
    if (result == ESP_OK) {
        state_.card_removed();
    }
    return result;
}

esp_err_t StickySdCard::cleanup_fault()
{
    if (!valid_board()) {
        return ESP_ERR_INVALID_ARG;
    }
    if (state_.state() != sticky_storage::SdCardState::Fault) {
        return ESP_ERR_INVALID_STATE;
    }
    if (state_.mounted()) {
        if (!state_.begin_eject()) {
            return ESP_ERR_INVALID_STATE;
        }
        return finish_eject();
    }

    const bool card_present = sample_present();
    esp_err_t result = board_.unmount();
    if (result == ESP_ERR_INVALID_STATE) {
        // The board has no mounted card or deferred pin cleanup. A failed mount
        // may still leave the lifecycle in Fault even though power-off worked.
        result = ESP_OK;
    }
    if (result == ESP_OK && !state_.clear_fault(card_present)) {
        return ESP_ERR_INVALID_STATE;
    }
    return result;
}

bool StickySdCard::mark_full() { return state_.mark_full(); }

bool StickySdCard::mark_space_available()
{
    return state_.mark_space_available();
}

bool StickySdCard::mark_read_only() { return state_.mark_read_only(); }

void StickySdCard::mark_fault() { state_.mark_fault(); }

sticky_storage::SdCardState StickySdCard::state() const
{
    return state_.state();
}

uint32_t StickySdCard::generation() const { return state_.generation(); }

bool StickySdCard::mounted() const { return state_.mounted(); }

bool StickySdCard::present() const { return presence_known_ && present_; }

bool StickySdCard::can_read() const { return state_.can_read(); }

bool StickySdCard::can_write() const { return state_.can_write(); }

bool StickySdCard::accepts(uint32_t request_generation, bool write) const
{
    return state_.accepts(request_generation, write);
}

esp_err_t StickySdCard::space(StickySdCardSpace &space) const
{
    space = {};
    if (!valid_board()) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!state_.mounted() || !state_.can_read()) {
        return ESP_ERR_INVALID_STATE;
    }
    const esp_err_t result =
        board_.space(space.total_bytes, space.free_bytes);
    if (result != ESP_OK || space.free_bytes > space.total_bytes) {
        space = {};
        return result == ESP_OK ? ESP_ERR_INVALID_SIZE : result;
    }
#if defined(STICKY_SD_INJECT_FULL) && STICKY_SD_INJECT_FULL
    space.free_bytes = 0U;
#endif
    return ESP_OK;
}

StickySdCard &sticky_sdcard_device()
{
    static StickySdCard device;
    return device;
}
