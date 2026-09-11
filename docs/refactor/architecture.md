# Sticky Core Framework architecture

## Runtime ownership

`app_registry.cpp` is the composition root. Each `StickyAppDescriptor` supplies
start, pause, resume, sleep preparation, timeout and sleep policy callbacks.
`StickyAppManager` validates IDs, starts the default Home app, switches pages,
and restores the previous page if activation fails. `sticky_app.cpp` owns only
framework input routing, launcher lifetime, deep sleep and display hand-off.

The coordinator never includes a concrete business page. New applications are
connected through the registry and remain independently testable.

## Device and display layers

Board power, charger, shared SPI, sensor I2C, GT911 touch, buttons, PCF8563 RTC,
BQ27220 battery, IMU, buzzer and the Seeed e-paper driver remain below the
runtime. Every app draws into the shared `Canvas`; the coordinator ensures only
one app owns it at a time. Home and Settings use landscape orientation. The
Pregnancy app keeps its existing landscape setup/dashboard flow.

E-paper policy is unchanged: normal pages may use partial refreshes, the first
boot performs a clean full refresh, and the final frame before deep sleep uses a
monochrome cleanup refresh before the display is put to sleep.

## Services

- `system/scheduler.*`: bounded event queue with update, cancel and next-event selection.
- `storage/persistent_state.*`: version, payload-size, sequence and FNV checksum validation.
- `storage/settings_store.*`: two NVS slots with newest-valid-record selection.
- `ui/ui_language.*`: one bilingual copy table shared by Home, Settings and Pregnancy.

## Input flow

Bottom-edge swipe or top-button single click opens the launcher. A launcher card
selection pauses the current app, activates the target, and then resumes the
target. Double-click always selects Home. A launcher timeout closes without
changing the app. Settings can request Home after its own Back action.
