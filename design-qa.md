# Sticky App Launcher Design QA

## Evidence

- Source visual truth: `/Users/mengdu/.codex/generated_images/01a00e64-6173-7c61-900b-598f2bc9b152/exec-fb66571d-5d17-4aca-9dc3-18fd3f684128.png`
- Portrait implementation: `assets/app_launcher/qa/launcher_portrait.png`
- Landscape implementation: `assets/app_launcher/qa/launcher_landscape.png`
- Combined comparison: `assets/app_launcher/qa/comparison.png`
- Source pixels: 971 x 1627 design board containing both orientations.
- Implementation pixels and viewport: 480 x 800 portrait and 800 x 480 landscape at device density 1.
- Normalization: the source portrait and landscape screen regions were cropped from the design board and resampled to the two physical viewport sizes before comparison.
- State: `PET` selected; the other four applications are available.

## Full-view comparison

The current launcher extends the approved visual language to five entries: a centered 2 + 2 + 1 portrait grid and a five-card landscape row, with the same centered title, dotted divider, sticker illustrations, clipped-corner labels, grayscale halftone, selected sparkle, and inverse selected label.

The ImageGen board does not preserve the hardware's exact landscape aspect ratio, so the firmware implementation follows the physical 800 x 480 canvas while retaining the visible proportions and spacing rhythm.

## Required fidelity surfaces

- Fonts and typography: the built-in Sticky pixel font matches the source family and uppercase treatment. The title uses scale 4 and labels use scale 2, preserving the intended hierarchy without wrapping or truncation.
- Spacing and layout rhythm: portrait uses centered 2 + 2 + 1 rows with equal columns; landscape uses five equal touch tracks. Labels share one size and baseline. Every visible sticker remains inside its touch track.
- Colors and visual tokens: the page uses white, black, and `LightGray` only. Selected state uses a black label with white text and a black sparkle marker.
- Image quality and asset fidelity: all five applications use dedicated 136 x 136 pixel illustrations with complete silhouettes. Black outlines and gray halftone are stored as separate 1-bit layers and render without scaling artifacts.
- Copy and content: `CHOOSE AN APP`, `PET`, `FOCUS`, `STATUS`, `ANSWERS`, and `BABY WEEK` use the established launcher language.

The actual-size full views keep all five illustrations and labels legible, so an additional focused crop is not required.

## Comparison history

### Iteration 1

- Finding: P2, the first implementation used 160 x 160 stickers and a scale-3 title, leaving more empty space than the selected source.
- Fix: enlarged every sticker to 176 x 176, raised the title to scale 4, repositioned the lower portrait row, and matched the landscape selected state to `PET`.
- Post-fix evidence: `assets/app_launcher/qa/comparison.png` shows the revised proportions in both orientations.

### Iteration 2

- Finding: P2, the portrait rows were distributed too far toward the top and bottom edges, so the four entries read as two separate bands instead of one application group.
- Fix: moved the first portrait row down 40 pixels and the second row up 40 pixels, preserving equal label spacing and a 16-pixel non-interactive gap between the touch tracks.
- Post-fix evidence: `assets/app_launcher/qa/comparison.png` shows the compact centered portrait group while the landscape layout remains unchanged.

### Iteration 3

- Finding: P2, the portrait title group remained close to the top edge after the entry grid moved toward the center, leaving the upper page rhythm disconnected.
- Fix: moved the portrait title, both title sparkles, and the dotted divider down 28 pixels as one unit. The landscape title coordinates remain unchanged.
- Post-fix evidence: `assets/app_launcher/qa/comparison.png` shows the title and 2 x 2 entry grid reading as one centered portrait composition.

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
- [x] Sticky Debug and Release firmware builds.

final result: passed

---

# First-boot tutorial visual QA

## Result

Passed. The six portrait tutorial pages have zero P0, P1, and P2 visual issues.

## Compared artifacts

- References: approved page images in `assets/onboarding/redraws/`
- Firmware previews: `assets/onboarding/previews/tutorial-page-1.png` through `tutorial-page-6.png`
- Host-rendered frames: `/tmp/onboarding_1.ppm` through `/tmp/onboarding_6.ppm`

## Checks

- All six complete panels are present in the approved order.
- Titles, illustrations, callouts, borders, and page content match the approved source; the runtime footer reflects the current navigation state.
- Every page keeps its approved 480 x 800 portrait proportions.
- The monochrome threshold keeps the main outlines, fine callout arrows, and small labels legible.
- The primary `BACK`, page, and `NEXT / START` row is visually separated from the secondary underlined `SKIP TUTORIAL` row; all touch regions remain distinct and the footer border stays continuous.

final result: passed
