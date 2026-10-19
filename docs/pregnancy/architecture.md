# Architecture

Pregnancy Journal is an application layer on the existing Sticky Core. Device
drivers remain independent from pregnancy business logic, and only the core
coordinator owns global input, display hand-off, app switching and sleep.

```text
App registry / coordinator
  Launcher · app lifecycle · wake selection · deep sleep
             |
Four apps
  Baby Week · Weight · Kicks · Settings
             |
Domain and content services
  Pregnancy · Content · Weight · Kick
             |
Sticky Core
  AppManager · Scheduler · lifecycle · Canvas · SD worker
             |
Device HAL
  RTC · e-paper · touch · buttons · battery · buzzer · power · IMU
```

## Runtime ownership

`src/app/app_registry.cpp` is the composition root. Each
`StickyAppDescriptor` supplies start, pause, resume, sleep preparation, timeout
and sleep policy callbacks. `StickyAppManager` starts Baby Week by default,
sanitizes invalid persisted app IDs, pauses the previous app before switching,
and restores it when activation fails. Each app renders through the shared
`Canvas`; inactive apps are paused and cannot consume display or touch input.

`src/app/sticky_app.cpp` is the framework coordinator. It owns launcher
gestures, top-button routing, deep sleep, timer wake selection, maintenance wake
selection and persistence of the last app. The coordinator never includes a
concrete business page, so apps remain independently testable.

## App boundaries

- **Baby Week** owns pregnancy profile setup, progress calculation and offline
  week content. Its pages support landscape and portrait.
- **Weight** owns daily records, kg/lb conversion, height/baseline settings and
  derived BMI/trend summaries.
- **Kicks** owns timed sessions, accidental-double-tap filtering, undo, daily
  summaries and seven-day recorded-data trends. Its presentation layer never
  classifies movement as normal or abnormal.
- **Settings** owns language, RTC editing, pregnancy-setup requests and e-paper
  cleanup. It may request a return to Baby Week but never switches apps itself.

All app data services use bounded arrays. The current capacities are
366 weight records and 400 kick sessions. The framework
app manager supports at most eight registered apps, leaving room for future
additions without changing persisted IDs.

## Navigation and input

- Top-button single click toggles Launcher; double-click selects Baby Week.
- A bottom-edge upward swipe opens Launcher. A downward swipe beginning in the
  lower 30% closes it.
- Touching a card switches apps only after the current app is paused.
- Launcher closes after 30 seconds without activity.
- A two-second chord on both side buttons requests deep sleep.
- Touch and button events stop an active wake buzzer before app handling.

The gesture classifier accepts a predominantly vertical swipe lasting 60–1600
ms and travelling at least 96 px (or 20% of the logical panel height).

## Display and power

The display owner remains in Sticky Core. Stable page entry and sleep cleanup
use a full monochrome refresh; app mutations use partial refresh. A cleanup full
refresh follows five fast app transitions. The final frame draws a sleep
indicator, powers down the panel and preserves the selected app/rotation in
RTC no-init context.

Stable pages time out after 60 seconds without external power. Settings uses a
three-minute timeout, while editors and an active kick session keep the device
awake. Before sleep, one coordinator chooses the closest future event from the
daily 03:00 maintenance refresh or an active-app request, then arms one ESP32
timer wake up to 15 seconds early. Business-alert wake/buzzer paths are disabled.

## Persistence and failure boundaries

Device settings, pregnancy profile, weights and Kicks use the dataset-bound
MicroSD repository.
Kicks has independent read-only import and verified cleanup; common cleanup
never erases the Kicks namespace. The shared recovery index
holds 1,024 keys including tombstones and has no garbage collector.

RTC or battery failures degrade to unavailable status rather than blocking
navigation. Invalid dates and invalid records are rejected at the service
boundary. No app owns a hardware bus, network connection, cloud log or AI
service.
