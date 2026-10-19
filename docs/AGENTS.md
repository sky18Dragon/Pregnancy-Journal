# Project agent guide

This repository contains the Pregnancy Journal firmware for Seeed Studio
reTerminal Sticky. Treat the code, not old screenshots or historical refactor
notes, as the source of truth. The current product is an offline organizer, not
a medical device.

## Start here

Read these files before changing behavior:

- `docs/PROJECT-SPEC.md`: next-version product scope and confirmed MicroSD storage requirements.
- `TODO.md`: development design, dependencies and task completion evidence.
- `README.md`: product scope, build and device controls.
- `DESIGN.md`: e-paper visual and interaction rules.
- `docs/ARCHITECTURE.md`: runtime ownership and dependency direction.
- `docs/PAGE-STRUCTURE.md`: page inventory and navigation.
- `docs/COMPONENT-GUIDELINES.md`: module contracts and extension rules.
- `docs/DEVELOPMENT.md`: build, test, flash and review workflow.
- `docs/HARDWARE-REFERENCE.md`: verified Sticky hardware facts and sources.

`docs/refactor/` contains useful history but is not the current product
specification. Prefer `docs/pregnancy/current-functionality.md` for shipped
behavior.

## Non-negotiable boundaries

- Keep dependency flow one-way: apps -> domain/content services -> Sticky Core
  -> board/device drivers.
- Register applications in `src/app/app_registry.cpp`; do not add direct
  app-to-app calls.
- Only the framework coordinator owns the launcher, app switching, display
  hand-off, deep sleep and global input routing.
- Pages render state into the shared `Canvas`. They must not read hardware,
  mount storage, switch apps or refresh the panel directly.
- Reuse the sensor I2C bus and shared SPI2 bus. Never create a competing bus on
  the same pins from an app or page.
- Keep records bounded, versioned and validated. User data persists on
  MicroSD according to `docs/PROJECT-SPEC.md`. Use the shared SD storage service and domain-specific
  repositories; do not add NVS user-data storage or silently fall back to Flash.
  Preserve existing NVS records through the validated migration process.
- Existing `StickyAppId` values are persistent data. Do not renumber or reuse
  retired IDs.
- Keep health copy informational. Do not introduce diagnosis, universal
  thresholds, emergency promises or treatment advice.

## UI and e-paper rules

- Support both logical sizes: 800×480 landscape and 480×800 portrait.
- Use `src/ui/ui_tokens.h` and helpers in `src/ui/prototype_ui.*`; do not create
  an unrelated spacing/type system inside one app.
- Visual geometry and hit geometry must be derived from the same layout.
- Use black/white for interactive pages unless a deliberate full grayscale
  refresh is justified.
- Use full monochrome refresh for stable entry/cleanup and partial refresh for
  small mutations. Repeated partial updates require a cleanup full refresh.
- Store editable artwork in `assets/`, generated firmware data in
  `src/ui/assets/`, and regenerate it with the matching `tools/generate_*.py`.
- Add Chinese glyphs through the font generator; do not silently fall back to
  missing glyph boxes.

## Implementation checklist

1. Locate the owning layer and follow a nearby implementation.
2. Keep hardware-independent rules in pure C++ so host tests can cover them.
3. Add or update focused tests for state transitions, boundaries and hit maps.
4. Run `./tools/run_host_tests.sh` and `python3 tools/check_markdown_links.py`.
5. Build `sticky-release`, `sticky-debug` and `sticky-power-test` when firmware
   sources or build configuration change.
6. For hardware, power, touch, orientation or refresh changes, complete the
   physical checklist in `docs/pregnancy/hardware-regression-checklist.md`.
7. Update user-facing documentation and `CHANGELOG.md` with shipped behavior,
   not intended behavior.

The workspace path contains spaces and PlatformIO may reject it. Follow the
no-space mirror procedure in `docs/DEVELOPMENT.md`; do not rename the user's
workspace as a workaround.
