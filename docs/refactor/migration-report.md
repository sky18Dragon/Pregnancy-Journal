# Migration report

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

Final release build after Pregnancy retention: 482,032 B linked flash and
16,916 B RAM. The final host suite has 14 focused tests covering manager
fallback/switching, launcher hit testing, persistence, scheduling, Home,
Settings, Pregnancy and hardware-independent input policies.

Hardware flashing is intentionally a separate action from compilation. The
device port previously detected in this workspace is
`/dev/cu.usbmodem5C843369951`; after the final build it should be flashed and
checked using the checklist below.
