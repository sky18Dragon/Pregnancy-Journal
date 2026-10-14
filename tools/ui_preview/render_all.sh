#!/usr/bin/env bash
# Build the host preview renderer and render every page (landscape/portrait x en/zh).
# Usage: tools/ui_preview/render_all.sh [output_dir] [name_filter]
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${1:-${ROOT}/build/ui_preview}"
BIN="${TMPDIR:-/tmp}/sticky_ui_preview"
cd "${ROOT}"
mkdir -p "${OUT}"
rm -f "${OUT}"/*.mono "${OUT}"/*.meta "${OUT}"/*.png "${OUT}"/*.raw
g++ -std=c++17 -Wall -Wextra -O1 \
  -Isrc/app -Isrc/apps/kick -Isrc/apps/pregnancy -Isrc/apps/settings -Isrc/apps/weight \
  -Isrc/pregnancy/content -Isrc/pregnancy/models -Isrc/pregnancy/services \
  -Isrc/ui -Isrc/ui/assets -Itest/support \
  tools/ui_preview/ui_preview.cpp \
  src/ui/app_pages.cpp src/ui/canvas.cpp src/ui/font.cpp src/ui/prototype_ui.cpp \
  src/ui/text_layout.cpp src/ui/ui_language.cpp \
  src/ui/assets/chinese_font_assets.cpp src/ui/assets/pixel_asset.cpp \
  src/ui/assets/app_launcher_assets.cpp src/ui/assets/pregnancy_week_assets.cpp \
  src/apps/kick/kick_pages.cpp src/apps/pregnancy/pregnancy_pages.cpp \
  src/apps/pregnancy/pregnancy_state.cpp src/apps/settings/settings_pages.cpp \
  src/apps/weight/weight_pages.cpp src/pregnancy/services/weight_service.cpp \
  src/pregnancy/services/kick_comparison.cpp src/pregnancy/services/kick_service.cpp \
  src/pregnancy/content/content_service.cpp src/pregnancy/content/pregnancy_week_visual.cpp \
  src/app/sticky_app_id.cpp \
  -o "${BIN}"
"${BIN}" "${OUT}" "${2:-}"
python3 -I tools/ui_preview/render_png.py "${OUT}"
