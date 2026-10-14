#pragma once
#include <cstdint>

#include "esp_err.h"

// Low-level board operations. Product runtime code must use sticky_sdcard;
// isolated S0 diagnostics may call these directly. No formatting or bus free.
bool board_sd_card_present();
esp_err_t board_sd_card_mount();
esp_err_t board_sd_card_unmount();
// Reports FAT capacity only while the board driver owns a mounted card.
// This is read-only and never creates application data.
esp_err_t board_sd_card_space(uint64_t &total_bytes, uint64_t &free_bytes);
