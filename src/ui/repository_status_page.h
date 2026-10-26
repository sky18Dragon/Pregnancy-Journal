#pragma once

#include "canvas.h"
#include "prototype_ui.h"
#include "sd_repository_adapter.h"

namespace repository_status_ui {

struct State {
    bool pending = false;
    bool saving = false;
    sticky_storage::SdRepositoryAdapterStatus status =
        sticky_storage::SdRepositoryAdapterStatus::Idle;
    uint32_t setup_generation = 0U;
};

enum class Action { None, Back, Retry, Setup };

// Pure snapshot policy: never probe or write the card from a renderer.
bool can_initialize(const sticky_storage::SdStorageServiceStatus &card);
State make_state(bool pending, bool saving,
                 sticky_storage::SdRepositoryAdapterStatus status,
                 const sticky_storage::SdStorageServiceStatus &card);
prototype_ui::Rect setup_rect(const Canvas &canvas);
Action action_at(const Canvas &canvas, const State &state, int x, int y);
prototype_ui::Rect target_rect(const Canvas &canvas, Action action);
prototype_ui::Rect retry_rect(const Canvas &canvas);
const char *message(sticky_storage::SdRepositoryAdapterStatus status);
void render(Canvas &canvas, const char *title, const State &state);

}  // namespace repository_status_ui
