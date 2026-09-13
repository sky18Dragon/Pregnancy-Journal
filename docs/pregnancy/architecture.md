# Architecture

Pregnancy Journal is an application layer on the existing Sticky Core. Device
drivers remain independent from pregnancy business logic.

```text
Pregnancy apps
  Baby Week / Reminder / Checkup / Settings
                 |
Domain and content services
  Pregnancy / Reminder / Checkup / Content
                 |
Sticky Core
  AppManager / Scheduler / lifecycle / display / input / persistence
                 |
Device HAL
  RTC / e-paper / touch / buttons / battery / buzzer / power
```

`app_registry.cpp` is the composition root. `StickyAppManager` owns the active
app lifecycle; inactive apps are paused and cannot consume display or touch.
The framework coordinator alone owns launcher gestures, app switching, deep
sleep and selection of the next system wake.

The product is offline-first. There is no account, network dependency,
telemetry, cloud log, AI service or third-party SDK. Fixed-capacity arrays and
fixed text buffers keep heap behavior predictable.

## Navigation

- Start and deep-sleep resume open the retained app, falling back to Baby Week.
- Swipe up from the bottom or single-click the top button opens Launcher.
- Swipe down or single-click closes Launcher.
- Double-click returns to Baby Week from any app.
- Pregnancy has Overview, Baby and Mom tabs.
- Settings provides date/time, display cleanup, language and pregnancy setup.

## Failure boundaries

RTC or battery failures degrade to unavailable UI. Corrupt profile records use
the redundant NVS fallback; corrupt reminder and checkup records are skipped.
