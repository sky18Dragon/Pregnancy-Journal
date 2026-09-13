# Migration report

This report is historical. Its final 2026-09-12 three-app record predates the
current six-app product scope. The live baseline is documented in
[`docs/pregnancy/baseline.md`](../pregnancy/baseline.md).

The refactor was executed as granular commits from the checkpoint at
`ffd8039`.

| Stage | Result |
| --- | --- |
| Baseline | Release 2,189,948 B linked flash; debug 2,193,712 B; power-test 2,193,920 B; 34/34 host tests. |
| Generic Home | Home became the default landscape app; 35/35 host tests; all three firmware profiles built. |
| Core runtime | Added registry/manager, Scheduler, PersistentState/SettingsStore and Settings; 39/39 host tests; release built. |
| App-only runtime | Coordinator and launcher reduced to framework-owned Home/Settings composition. |
| Business removal | Desktop Pet, Answers, Status Board, Pomodoro and onboarding code/assets/tests removed. |
| Pregnancy retention | Pregnancy app restored as the third registered app with its setup/dashboard tests. |

Final release build after Pregnancy retention: 427,576 B flash, 16,916 B RAM,
and a 428,240 B `firmware.bin`. The final host suite has 14 focused tests covering manager
fallback/switching, launcher hit testing, persistence, scheduling, Home,
Settings, Pregnancy and hardware-independent input policies.

Firmware `1.0.0` at commit `fdc6be6` was flashed through
`/dev/cu.usbmodem5C843369951` and verified on 2026-09-12. Serial logs reached
`phase=ready`, reported `apps=3 default=home current=home`, and confirmed the
RTC, battery, display, touch, IMU and button paths. The user completed the
physical interaction checklist without finding a problem.

## Subsequent product-scope update

The current branch extends the retained Pregnancy app with Reminders, Checkups,
Weight and Kicks, and keeps Settings as the sixth registered app. Commit
`2556df8` (`2.0.0`) passes 20/20 host tests and a release build. A fresh
physical-device regression for this image is still required; the archived
three-app pass above must not be used as evidence for the new applications.
