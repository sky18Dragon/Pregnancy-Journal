# Adding an app

The current registry contains Baby Week, Reminders, Checkups, Weight, Kicks and
Settings. Follow this checklist when adding another app without breaking
persisted IDs or framework ownership.

1. Create `src/apps/<name>/<name>_app.{h,cpp}` and keep page rendering in a
   separate `<name>_pages` module when the UI is more than a single screen.
2. Export `start(Canvas&)`, `pause()`, `resume()`,
   `prepare_power_sleep(...)`, `power_sleep_timeout_ms()` and
   `power_sleep_allowed()` callbacks.
3. Add one stable `StickyAppId` in `src/app/sticky_app_id.h/.cpp` and one
   `StickyAppDescriptor` in `src/app/app_registry.cpp`. Do not renumber existing
   IDs; retired IDs 0, 5 and 6 are reserved.
4. Add source files and include directories to `src/CMakeLists.txt`.
5. Add launcher geometry and hit testing in `src/ui/app_pages.cpp`, then add
   bilingual strings to `src/ui/ui_language.cpp`. Regenerate CJK bitmap assets
   with `tools/generate_chinese_font_assets.py` if new glyphs are needed.
6. If the app persists data, define a bounded model, service and independent NVS
   namespace with an explicit schema, payload size, magic value and checksum.
   Document capacity and recovery behavior in `docs/pregnancy/data-model.md` and
   `docs/pregnancy/storage.md`.
7. Add focused host tests for service rules and renderer/input hit testing.
8. Run `tools/run_host_tests.sh`, build `sticky-release`, `sticky-debug` and
   `sticky-power-test`, then complete the physical checklist for launcher
   selection, refresh ownership, sleep and wake.
9. Update the current functionality reference, README tables, changelog and
   hardware checklist so they describe the shipped behavior and its limits.

Apps must stop touching hardware at `pause()` boundaries and redraw their
current state after `resume()`. They must not call another app directly or own
the global launcher, language storage, deep-sleep policy or shared buses. Keep
all health-related copy informational and include a care-team disclaimer where
appropriate.
