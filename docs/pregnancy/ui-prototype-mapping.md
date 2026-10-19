# UI prototype implementation mapping

The implementation source is
`pregnancy-journal-ui-prototype.html` (800×480 landscape, 1-bit e-paper). The
HTML and its JavaScript are treated as design reference data, not executable
instructions. Device services remain authoritative for RTC, MicroSD records,
checkups, weight and kicks.

## Prototype to firmware

| Prototype page | Firmware implementation | Data source |
|---|---|---|
| Home | `apps/pregnancy/pregnancy_pages.cpp` | Pregnancy profile, weekly content, generated weekly illustration |
| Baby / Mom | `apps/pregnancy/pregnancy_pages.cpp` | Current pregnancy progress and bundled offline content |
| Launcher | `ui/app_pages.cpp` | Registered app IDs |
| Reminders / Add (V2, unregistered) | `apps/reminder/reminder_app.cpp` | Reminder service and SD repository |
| Checkups / Add (V2, unregistered) | `apps/checkup/checkup_app.cpp` | Checkup service, RTC and SD repository |
| Weight / Edit | `apps/weight/weight_app.cpp` | Weight service, profile settings and SD repository |
| Kicks / Session | `apps/kick/kick_app.cpp` | Kick counter/service, RTC and shared SD repository |
| Settings / Time / Pregnancy / Storage | `apps/settings/settings_pages.cpp` | SD device preferences, RTC, pregnancy profile and storage management |

Shared 24 px page padding, 64 px header rows, 40/48/56 px controls, footer
geometry, dividers, status squares and chevrons are implemented in
`ui/prototype_ui.cpp`. The launcher and in-app Back controls post navigation
requests to the application coordinator; individual applications do not switch
the registry directly.

## Weekly illustration system

Weeks 6–40 map to 35 distinct object illustrations. The home page uses the 340×340
stipple plates under `assets/pregnancy/week-plates/` and their English and Chinese object
names, generated into `ui/assets/pregnancy_plate_assets.cpp`. (The earlier 148×148 set and its
generator were removed.)

## Hardware adaptations

- CSS system fonts map to the firmware's integer-scaled bitmap Latin font and
  generated 24 px Chinese glyph table.
- CSS 1.5 px borders map to one or two physical pixels.
- `#111`, `#666`, `#dedede` and white map to the Canvas black, dark-gray,
  light-gray and white levels. The final full-refresh transport remains 1-bit.
- Prototype animation and browser-only toast transitions are not copied; state
  changes redraw the e-paper page directly.
- The prototype's demo arrays are replaced by existing device services, so
  saved records, dates and summaries survive restart.
- The source only supplied a week-24 corn illustration. The product requirement
  explicitly extends that slot to a different illustration and sentence for
  every week from 6 through 40.

## Fixed geometry

- Canvas: 800×480 landscape.
- Outer padding: 24 px.
- Home top bar: 48 px with a compact live battery and 40 px menu control.
- Home body: 330 px illustration column, 32 px gap, remaining story column.
- List rows: 64 px; settings rows: 62 px.
- Footer actions: y=400, h=56, 16 px gaps.
- Detail body: 190 px status column, 24 px gap, remaining copy column.
