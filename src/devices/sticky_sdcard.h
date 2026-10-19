#pragma once

#include <cstdint>

#include "esp_err.h"
#include "sd_card_state.h"

struct StickySdCardBoardOps {
    bool (*present)() = nullptr;
    esp_err_t (*mount)() = nullptr;
    esp_err_t (*unmount)() = nullptr;
    esp_err_t (*space)(uint64_t &total_bytes, uint64_t &free_bytes) = nullptr;
};

struct StickySdCardSpace {
    uint64_t total_bytes = 0U;
    uint64_t free_bytes = 0U;
};

enum class StickySdCardEvent : uint8_t {
    None = 0,
    Inserted,
    Removed,
    RemovalCleanupFailed,
};

struct StickySdCardPollResult {
    StickySdCardEvent event = StickySdCardEvent::None;
    esp_err_t error = ESP_OK;
};

// Single-owner runtime adapter for the board MicroSD driver. Call its methods
// serially from the future storage coordinator task, never from an ISR.
// MicroSD运行时适配器由存储协调任务串行调用，禁止在中断中执行。
class StickySdCard {
  public:
    StickySdCard();
    explicit StickySdCard(const StickySdCardBoardOps &board);

    esp_err_t mount();
    StickySdCardPollResult poll();

    // Phase one rejects new requests. The storage owner must drain and close
    // admitted work before calling finish_eject().
    // 第一阶段停止接收新请求；排空并关闭已接纳任务后才能完成卸载。
    esp_err_t begin_eject();
    esp_err_t finish_eject();

    // Retries low-level cleanup after a physically removed card could not be
    // unmounted. This never reports success while the card is still present.
    esp_err_t recover_removed_card();

    // Finishes any low-level cleanup left by a mount or unmount failure. A
    // mounted fault is safely ejected; an unmounted fault retries board cleanup.
    esp_err_t cleanup_fault();

    void invalidate_requests() { state_.invalidate_requests(); }

    bool mark_full();
    bool mark_space_available();
    bool mark_read_only();
    void mark_fault();

    sticky_storage::SdCardState state() const;
    uint32_t generation() const;
    bool mounted() const;
    bool present() const;
    bool can_read() const;
    bool can_write() const;
    bool accepts(uint32_t request_generation, bool write) const;
    esp_err_t space(StickySdCardSpace &space) const;

  private:
    bool valid_board() const;
    bool sample_present();

    StickySdCardBoardOps board_;
    sticky_storage::SdCardStateMachine state_;
    bool presence_known_ = false;
    bool present_ = false;
};

// Production singleton backed by board_sd_card.*.
StickySdCard &sticky_sdcard_device();
