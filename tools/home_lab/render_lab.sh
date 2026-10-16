#!/usr/bin/env bash
# Build and run the home-page design lab, then lint and assemble the viewer.
# Usage: tools/home_lab/render_lab.sh [weeks|all] [concept_filter] [output_dir] [solid|line|riso|hand|engrave|pixel]
#   weeks   comma list (6..41) or "all"; default 6,12,21,28,34,40,41
#   output  default build/home_lab (open lab.html)
# Host preview only: nothing here reflects refresh behaviour or ghosting.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
WEEKS="${1:-6,12,21,28,34,40,41}"
FILTER="${2:-}"
ART="${4:-solid}"   # solid | line | pixel (illustration style)
OUT="${3:-${ROOT}/build/home_lab}"
BIN="${TMPDIR:-/tmp}/sticky_home_lab"
cd "${ROOT}"
mkdir -p "${OUT}" "${OUT}/gen"
[ "${ART}" = "engrave" ] || [ "${ART}" = "pixel" ] || python3 -I tools/home_lab/geo_art.py assets/concepts/geo-week-size --styles "${ART}" >/dev/null
ART_DIR="assets/concepts/geo-week-size/${ART}"
LABELS=""
[ "${ART}" = "pixel" ] && ART_DIR="/nonexistent"
if [ "${ART}" = "engrave" ]; then
  # engraving/stipple art cut from the owner's reference sheet (host prototype only)
  SHEET="${ENGRAVE_SHEET:-assets/concepts/references/week-fruit-engraving-sheet.webp}"
  python3 -I tools/home_lab/stipple_art.py "${OUT}/engraving" --sheet "${SHEET}" --size "${ENGRAVE_SIZE:-340}" >/dev/null
  ART_DIR="${OUT}/engraving"; LABELS="${OUT}/engraving/labels.tsv"
fi
python3 -I tools/home_lab/make_display_assets.py "${OUT}/gen" --art-dir "${ART_DIR}"
rm -f "${OUT}"/*.mono "${OUT}"/*.meta "${OUT}"/*.png "${OUT}"/*.raw
g++ -std=c++17 -Wall -Wextra -O1 -DUI_TEXT_OBSERVER \
  -I"${OUT}/gen" -Isrc/ui -Isrc/ui/assets -Isrc/pregnancy/content -Itest/support \
  tools/home_lab/home_lab.cpp \
  src/ui/canvas.cpp src/ui/font.cpp src/ui/prototype_ui.cpp src/ui/text_layout.cpp \
  src/ui/ui_language.cpp \
  src/ui/assets/chinese_font_assets.cpp src/ui/assets/pixel_asset.cpp \
  src/ui/assets/pregnancy_week_assets.cpp \
  src/pregnancy/content/content_service.cpp src/pregnancy/content/pregnancy_week_visual.cpp \
  -o "${BIN}"
"${BIN}" "${OUT}" tools/home_lab/week_size_reference.tsv "${WEEKS}" "${FILTER}" "${ART}" "${LABELS}"
python3 -I tools/ui_preview/render_png.py "${OUT}"
python3 -I tools/home_lab/build_viewer.py "${OUT}"
