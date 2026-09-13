# Sticky Core Framework architecture

This page documents the framework boundary used by the current product. The
current user-facing feature inventory is maintained in
[`docs/pregnancy/current-functionality.md`](../pregnancy/current-functionality.md).

## Runtime ownership

`app_registry.cpp` is the composition root for six descriptors: Baby Week,
Reminders, Checkups, Weight, Kicks and Settings. Each descriptor supplies
start, pause, resume, sleep preparation, timeout and sleep-policy callbacks.
`StickyAppManager` validates IDs, starts Baby Week by default, switches apps,
and restores the previous app if activation fails. The coordinator in
`sticky_app.cpp` owns only framework input routing, launcher lifetime, app
persistence, deep sleep and display hand-off.

The coordinator never includes a concrete business page. New applications are
connected through the registry and remain independently testable.

## Device and display layers

Board power, charger, shared SPI, sensor I2C, GT911 touch, buttons, PCF8563 RTC,
BQ27220 battery, IMU, buzzer and the Seeed e-paper driver remain below the
runtime. Every app draws into the shared `Canvas`; the coordinator ensures only
one app owns it at a time. All current apps use fixed landscape orientation.

E-paper policy is coordinator/driver-owned: stable entry and sleep use full
monochrome refreshes, mutations use partial refreshes, and repeated fast
transitions trigger cleanup refreshes. Settings exposes an explicit cleanup
action.

## Services and persistence

- `system/scheduler.*`: bounded typed event queue and closest-event selection.
- `storage/persistent_state.*` and `storage/settings_store.*`: versioned,
  checksummed alternating device settings records.
- `apps/pregnancy` + `pregnancy/content`: profile, calculation and offline week
  content.
- `pregnancy/services`: bounded reminder, checkup, weight and kick services plus
  independent NVS storage.
- `ui/ui_language.*`: shared English/Simplified Chinese translation table.

The framework manager supports at most eight apps. Current service capacities
are 12 reminders, 8 checkups, 32 weight records and 42 kick sessions.

## Input flow

Bottom-edge swipe or top-button single click opens the launcher. A launcher card
selection pauses the current app, activates the target and resumes the target.
Double-click always selects Baby Week. A launcher timeout closes without
changing the app. Settings can request Baby Week after its own Back action.

Apps must stop touching hardware at `pause()` boundaries and redraw current
state after `resume()`. They must not call another app directly or own the global
launcher, language storage, deep-sleep policy or shared buses.
