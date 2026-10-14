#include <cassert>

#include "sticky_sdcard.h"

namespace {

bool card_present = false;
esp_err_t mount_result = ESP_OK;
esp_err_t unmount_result = ESP_OK;
int mount_calls = 0;
int unmount_calls = 0;
esp_err_t space_result = ESP_OK;
uint64_t total_bytes = 1000U;
uint64_t free_bytes = 400U;

bool fake_present() { return card_present; }

esp_err_t fake_mount()
{
    ++mount_calls;
    return mount_result;
}

esp_err_t fake_unmount()
{
    ++unmount_calls;
    return unmount_result;
}

esp_err_t fake_space(uint64_t &total, uint64_t &free)
{
    if (space_result == ESP_OK) {
        total = total_bytes;
        free = free_bytes;
    }
    return space_result;
}

const StickySdCardBoardOps kFakeBoard = {
    fake_present,
    fake_mount,
    fake_unmount,
    fake_space,
};

}  // namespace

// Satisfy the production singleton's low-level symbols without constructing it.
bool board_sd_card_present() { return fake_present(); }
esp_err_t board_sd_card_mount() { return fake_mount(); }
esp_err_t board_sd_card_unmount() { return fake_unmount(); }
esp_err_t board_sd_card_space(uint64_t &total, uint64_t &free)
{
    return fake_space(total, free);
}

int main()
{
    using sticky_storage::SdCardState;

    StickySdCard invalid({nullptr, nullptr, nullptr, nullptr});
    assert(invalid.mount() == ESP_ERR_INVALID_ARG);
    assert(invalid.poll().error == ESP_ERR_INVALID_ARG);
    StickySdCardSpace capacity = {};
    assert(invalid.space(capacity) == ESP_ERR_INVALID_ARG);

    StickySdCard card(kFakeBoard);
    assert(card.mount() == ESP_ERR_NOT_FOUND);
    assert(card.state() == SdCardState::Absent);
    assert(!card.present() && mount_calls == 0);

    card_present = true;
    StickySdCardPollResult update = card.poll();
    assert(update.event == StickySdCardEvent::Inserted);
    assert(update.error == ESP_OK && card.present());
    assert(card.mount() == ESP_OK);
    assert(mount_calls == 1 && card.generation() == 1U);
    assert(card.state() == SdCardState::ReadyReadWrite);
    assert(card.space(capacity) == ESP_OK);
    assert(capacity.total_bytes == total_bytes &&
           capacity.free_bytes == free_bytes);
    free_bytes = total_bytes + 1U;
    assert(card.space(capacity) == ESP_ERR_INVALID_SIZE);
    assert(capacity.total_bytes == 0U && capacity.free_bytes == 0U);
    free_bytes = 400U;
    space_result = ESP_FAIL;
    assert(card.space(capacity) == ESP_FAIL);
    space_result = ESP_OK;
    assert(card.accepts(1U, false) && card.accepts(1U, true));
    assert(card.mount() == ESP_ERR_INVALID_STATE);

    assert(card.mark_full());
    assert(card.can_read() && !card.can_write());
    assert(card.mark_space_available());
    assert(card.mark_read_only());
    assert(card.state() == SdCardState::ReadyReadOnly);
    assert(card.can_read() && !card.can_write());

    assert(card.begin_eject() == ESP_OK);
    assert(card.state() == SdCardState::Ejecting);
    assert(!card.accepts(1U, false));
    unmount_result = ESP_FAIL;
    assert(card.finish_eject() == ESP_FAIL);
    assert(card.state() == SdCardState::Fault && card.mounted());
    assert(card.begin_eject() == ESP_OK);
    unmount_result = ESP_OK;
    assert(card.finish_eject() == ESP_OK);
    assert(card.state() == SdCardState::Unmounted && !card.mounted());
    assert(card.space(capacity) == ESP_ERR_INVALID_STATE);

    // A safely ejected card stays unmounted until explicitly mounted again.
    update = card.poll();
    assert(update.event == StickySdCardEvent::None);
    assert(card.mount() == ESP_OK);
    assert(card.generation() == 2U);

    card_present = false;
    update = card.poll();
    assert(update.event == StickySdCardEvent::Removed);
    assert(update.error == ESP_OK);
    assert(card.state() == SdCardState::Absent && !card.mounted());
    assert(!card.accepts(2U, false));

    card_present = true;
    update = card.poll();
    assert(update.event == StickySdCardEvent::Inserted);
    assert(card.mount() == ESP_OK);
    assert(card.generation() == 3U);

    // Removal revokes access immediately even when low-level cleanup fails.
    card_present = false;
    unmount_result = ESP_FAIL;
    update = card.poll();
    assert(update.event == StickySdCardEvent::RemovalCleanupFailed);
    assert(update.error == ESP_FAIL);
    assert(card.state() == SdCardState::Fault && card.mounted());
    assert(!card.can_read() && !card.can_write());
    unmount_result = ESP_OK;
    assert(card.recover_removed_card() == ESP_OK);
    assert(card.state() == SdCardState::Absent && !card.mounted());

    card_present = true;
    update = card.poll();
    assert(update.event == StickySdCardEvent::Inserted);
    mount_result = ESP_FAIL;
    assert(card.mount() == ESP_FAIL);
    assert(card.state() == SdCardState::Fault && !card.mounted());
    assert(card.generation() == 4U);
    unmount_result = ESP_ERR_INVALID_STATE;
    assert(card.cleanup_fault() == ESP_OK);
    assert(card.state() == SdCardState::Unmounted);
    card_present = false;
    update = card.poll();
    assert(update.event == StickySdCardEvent::Removed);
    assert(card.state() == SdCardState::Absent);

    assert(unmount_calls == 6);

    card_present = true;
    StickySdCard initial_present(kFakeBoard);
    update = initial_present.poll();
    assert(update.event == StickySdCardEvent::Inserted);
    assert(initial_present.present());
}
