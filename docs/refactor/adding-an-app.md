# Adding an app

1. Create `src/apps/<name>/<name>_app.{h,cpp}` and keep page rendering in a
   separate `<name>_pages` module.
2. Export `start(Canvas&)`, `pause()`, `resume()`, `prepare_power_sleep(...)`,
   `power_sleep_timeout_ms()` and `power_sleep_allowed()` callbacks.
3. Add one `StickyAppId` and one descriptor in `src/app/app_registry.cpp`.
   The descriptor is the only place that binds concrete app code to the core.
4. Add the source files and include directory to `src/CMakeLists.txt`.
5. Add bilingual strings to `ui_language.cpp`, regenerate the CJK bitmap with
   `tools/generate_chinese_font_assets.py`, and add a focused host render/state test.
6. Build all three profiles, run `tools/run_host_tests.sh`, then run the manual
   hardware checklist for launcher selection, refresh ownership, sleep and wake.

Apps must stop touching hardware at `pause()` boundaries and must redraw their
current page after `resume()`. They must not call another app directly or own
the global launcher, language storage, deep-sleep policy, or shared buses.
