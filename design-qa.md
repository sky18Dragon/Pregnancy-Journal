# Sticky App Launcher Design QA

## Evidence

- Source visual truth: `/Users/mengdu/.codex/generated_images/01a00e64-6173-7c61-900b-598f2bc9b152/exec-fb66571d-5d17-4aca-9dc3-18fd3f684128.png`
- Portrait implementation: `assets/app_launcher/qa/launcher_portrait.png`
- Landscape implementation: `assets/app_launcher/qa/launcher_landscape.png`
- Combined comparison: `assets/app_launcher/qa/comparison.png`
- Source pixels: 971 x 1627 design board containing both orientations.
- Implementation pixels and viewport: 480 x 800 portrait and 800 x 480 landscape at device density 1.
- Normalization: the source portrait and landscape screen regions were cropped from the design board and resampled to the two physical viewport sizes before comparison.
- State: `PET` selected; the other three applications are available.

## Full-view comparison

The combined comparison confirms the same four-entry hierarchy, 2 x 2 portrait grid, single-row landscape grid, centered title, dotted divider, sticker illustrations, clipped-corner labels, grayscale halftone, selected sparkle, and inverse selected label.

The ImageGen board does not preserve the hardware's exact landscape aspect ratio, so the firmware implementation follows the physical 800 x 480 canvas while retaining the visible proportions and spacing rhythm.

## Required fidelity surfaces

- Fonts and typography: the built-in Sticky pixel font matches the source family and uppercase treatment. The title uses scale 4 and labels use scale 2, preserving the intended hierarchy without wrapping or truncation.
- Spacing and layout rhythm: portrait uses a compact centered 2 x 2 cluster with equal columns; landscape uses four equal touch tracks. Labels share one size and baseline. Every visible sticker remains inside its touch track.
- Colors and visual tokens: the page uses white, black, and `LightGray` only. Selected state uses a black label with white text and a black sparkle marker.
- Image quality and asset fidelity: all four applications use dedicated 176 x 176 generated pixel illustrations with complete silhouettes. Black outlines and gray halftone are stored as separate 1-bit layers and render without scaling artifacts.
- Copy and content: `CHOOSE AN APP`, `PET`, `FOCUS`, `STATUS`, and `ANSWERS` match the selected design.

The actual-size full views keep the four illustrations and all labels legible, so an additional focused crop is not required.

## Comparison history

### Iteration 1

- Finding: P2, the first implementation used 160 x 160 stickers and a scale-3 title, leaving more empty space than the selected source.
- Fix: enlarged every sticker to 176 x 176, raised the title to scale 4, repositioned the lower portrait row, and matched the landscape selected state to `PET`.
- Post-fix evidence: `assets/app_launcher/qa/comparison.png` shows the revised proportions in both orientations.

### Iteration 2

- Finding: P2, the portrait rows were distributed too far toward the top and bottom edges, so the four entries read as two separate bands instead of one application group.
- Fix: moved the first portrait row down 40 pixels and the second row up 40 pixels, preserving equal label spacing and a 16-pixel non-interactive gap between the touch tracks.
- Post-fix evidence: `assets/app_launcher/qa/comparison.png` shows the compact centered portrait group while the landscape layout remains unchanged.

## Findings

No actionable P0, P1, or P2 differences remain.

## Follow-up polish

- P3: physical e-paper contrast may make the light-gray halftone appear lighter than the computer preview; this is best judged on the device after flashing.

## Implementation checklist

- [x] Dedicated visual asset for every application.
- [x] Portrait and landscape layouts.
- [x] Dynamic selected application styling.
- [x] Large invisible touch targets aligned with visible entries.
- [x] Host render and hit-area regression coverage.
- [x] Sticky Debug firmware build.

final result: passed
