#pragma once

#include "canvas.h"
#include "prototype_ui.h"
#include "sd_repository_adapter.h"

namespace repository_status_ui {

prototype_ui::Rect retry_rect(const Canvas &canvas);
const char *message(sticky_storage::SdRepositoryAdapterStatus status);
void render(Canvas &canvas, const char *title, bool pending, bool saving,
            sticky_storage::SdRepositoryAdapterStatus status);

}  // namespace repository_status_ui
