# Component guidelines

Here “component” means a firmware module with one owner and a narrow public
contract, not a browser widget. Prefer small `.h/.cpp` pairs and pure functions
for rules that can be tested on the host.

## Module contracts

### Pages

- Accept prepared state and draw into `Canvas`.
- Export hit testing that uses the same rectangles as rendering.
- Support both 800×480 and 480×800 logical sizes.
- Do not access RTC, NVS, sensors, buses, the registry or display refresh APIs.
- Keep page/action enums close to their renderer interface.

### Apps

- Own current page, edit buffers and interaction flow.
- Load domain state on start/resume and persist after successful mutations.
- Implement lifecycle hooks from `StickyAppDescriptor`.
- Decide full versus partial refresh after rendering.
- Request framework navigation through the supported coordinator boundary; do
  not include another concrete app.

### Domain services

- Keep validation, sorting, calculations and capacity behavior independent of
  ESP-IDF when possible.
- Use fixed-width values and bounded character arrays.
- Make date/unit conversions explicit and test edge cases.
- Return errors instead of silently truncating or overwriting unrelated data.

### Storage adapters

- Pregnancy, reminder, checkup, weight, device preference and network
  authorization data use domain-specific repositories backed by the shared
  MicroSD storage service. Follow [TODO.md](../TODO.md) for
  migration, card identity, recovery and the limited device-only NVS exceptions.
- Define magic, schema version, payload size and checksum where records need
  recovery guarantees.
- Validate data before exposing it to an app. Skip corrupt sibling records.
- Document capacity, migration and erase behavior in `pregnancy/storage.md`.

### Board and device drivers

- Board modules own shared buses and power rails; device modules attach to them.
- Initialization and reads return `esp_err_t` and leave explicit degraded state.
- Do not duplicate pin numbers outside `src/board/pin_config.h`.
- Keep slow I/O outside callbacks and interrupt context.
- Coordinate e-paper and MicroSD access because they share SPI2.

### Shared UI

- Reuse `ui_tokens.h`, `prototype_ui.*`, `text_layout.*`, `font.*` and
  `ui_language.*` before adding local equivalents.
- Keep touch targets at least 48 px high and align them with visible controls.
- Add bilingual text and generated glyphs together.
- Render charts and status without relying on color.

## Naming and style

- Files, identifiers, logs and source comments use clear English.
- Public hardware interfaces include concise English/Chinese comments where the
  existing module follows that convention.
- Constants use `kPascalCase`; types use `PascalCase`; functions and local
  variables use `snake_case`, matching surrounding code.
- Avoid heap-backed containers in persistent/runtime hot paths unless the
  ownership and failure behavior are justified.
- Keep C++17 compatibility and compile cleanly with `-Wall -Wextra -Werror` in
  host tests.

## Adding an app

1. Add `src/apps/<name>/<name>_app.{h,cpp}` and separate page files when useful.
2. Add a stable, non-reused ID to `sticky_app_id.*`.
3. Add one descriptor in `app_registry.cpp`.
4. Add sources/include paths to `src/CMakeLists.txt`.
5. Add launcher geometry, bilingual labels and required glyphs/assets.
6. Add models/services/storage with explicit capacity if data is persisted.
7. Test lifecycle failure, page actions, rotation and domain boundaries.
8. Update page, architecture, storage, README and changelog documentation.

## Review questions

- Is there exactly one owner for the resource or state?
- Can the core rule run in a host test?
- Does failure preserve previously valid data and navigation?
- Are orientation, bilingual copy and hit targets consistent?
- Is the refresh choice appropriate for e-paper and repeated use?
- Does the change respect offline privacy and non-medical product boundaries?
