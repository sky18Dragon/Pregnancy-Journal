# Page and navigation structure

This inventory describes the current code, not future concepts. All registered
apps follow device orientation and render dedicated landscape/portrait layouts.

The four-app candidate implements [PROJECT-SPEC.md](PROJECT-SPEC.md). Physical
acceptance remains open in [V1 validation](V1-VALIDATION.md).

```text
Boot
  -> Baby Week setup (first use only)
  -> Baby Week Dashboard
       <-> Baby detail
       <-> Mom detail
  -> Launcher
       -> Baby Week
       -> Weight
       -> Kicks
       -> Settings
```

## Header action semantics

| Label | Meaning |
| --- | --- |
| APPS | Root page of Weight / Kicks / Settings: leave the app for the Launcher |
| BACK | One level up inside the app (see each app's back graph) |
| CANCEL | Abandon an in-progress entry or counter without saving |
| HOME | Baby Week home (detail pages, pregnancy summary) |

Back graphs live in `kick_page_back_target()`, `weight_page_back_target()` and
`settings_page_back_target()`. Kicks: Feel → Record type → Today; Compare returns
to the page it was opened from (Trend or Result).

## Global navigation

| Input | Result |
| --- | --- |
| Top button single click | Toggle Launcher |
| Top button double click | Return to Baby Week |
| Bottom-edge upward swipe | Open Launcher |
| Downward swipe beginning in lower 30% | Close Launcher |
| Bottom-edge swipe left / right (Launcher closed) | Switch to the next / previous app in Launcher order (Baby Week → Weight → Kicks → Settings, wrapping); a bar along the bottom edge acknowledges the swipe |
| Launcher card tap | Pause current app and activate selected app |
| Two side buttons held for two seconds | Request deep sleep |

Launcher inactivity closes it after 30 seconds. Stable app pages normally sleep
after 60 seconds on battery; Settings uses three minutes. Editors and an active
kick session can veto timeout sleep.

## Baby Week

| Page | Purpose | Primary transitions |
| --- | --- | --- |
| Clock setup | First-use RTC entry (`YYYYMMDDHHMM`) | Continue -> source setup |
| Source setup | Choose confirmed due date or LMP | Choice -> date setup; optional Back |
| Date setup | Enter `YYYYMMDD` and validate profile | Save -> Dashboard; Back |
| Dashboard | Week/day, trimester, due countdown, progress and week image; three visible "this week" rows with chevrons | Baby row → Baby; mother and advice rows → Mom; 56 px menu box → Launcher. The illustration and card header are not tappable |
| Baby | Offline weekly development content | Overview, Mom, Launcher |
| Mom | Offline weekly body/preparation content | Overview, Baby, Launcher |

`PregnancyPage` and `PregnancyAction` are declared in
`src/apps/pregnancy/pregnancy_pages.h`. Content is clamped to weeks 1–40; week
illustrations currently cover weeks 6–40.

## Weight

- Dashboard: latest value, baseline change, 7/30 calendar-day chart, unit switch.
- History: five rows per page in newest-first order; tap to edit that date.
- Editor: adjust kg/lb number, save or cancel; existing records may be deleted.
- Profile: edit height and baseline.
- Replace/delete confirmation: names the affected date before committing.

The renderer and hit maps share geometry in `apps/weight/weight_pages.*`.
There are at most 366 live records; same-day replacement retains the stable ID.

## Kicks

| Page | Purpose | Primary transitions |
| --- | --- | --- |
| Today | Today total, sessions, periods and latest session | Start, Insights, Back |
| Insights | Seven-day recorded-data chart and summaries | Start, Compare, Back |
| Record type | 1-hour count, Count to 10 or Feel check-in | Choose, Back |
| Counter | Current mode and period, elapsed time and count (n/10 for Count to 10) | Count, Undo, Finish, Cancel |
| Feel check-in | As usual, weaker, fewer, pattern changed | Choose (saves), Back |
| Result | Saved record against its seven-day median | Done, Compare, Back |
| Compare | Today per period, latest Count to 10, seven-day check-ins | Start, Back |

The Counter remains awake, filters taps closer than 400 ms and auto-finishes at
60 minutes. The UI reports recorded observations and always avoids diagnostic
classification.

## Settings

| Page | Purpose | Primary transitions |
| --- | --- | --- |
| Main | Language, RTC, pregnancy summary, display cleanup, MicroSD | Open editor/summary/storage; Home header action |
| Time editor | Edit and validate `YYYYMMDDHHMM` | Save or Back |
| Pregnancy summary | Source, due date, week and progress | Edit or Back |
| MicroSD summary | Card/access state, capacity, dataset identity, safe eject and data management | Initialize/eject, old-data import, dataset delete, device reset or Back |
| MicroSD confirmation | Explain the exact `PREGJNL` creation boundary | Create dataset, Cancel or Back |
| Import selection | Independent profile/weight/settings and Kicks sources | Select source or Back |
| Old device data | Import report for the selected NVS source | Scan/import, clear verified copies or Back |
| Old-copy cleanup confirmation | Name the exact legacy cleanup boundary | Clear verified copies, Cancel or Back |
| Card-data delete confirmation | Delete only the current card's `PREGJNL` directory | Delete app data, Cancel or Back |
| Device-binding reset confirmation | Forget only the local network key and RAM Wi-Fi | Reset this device, Cancel or Back |

Settings can request a return to Baby Week but does not switch the registry
itself. The time editor disables timeout sleep to protect in-progress input.
MicroSD initialization is never automatic. The summary enables its action only
for a writable, mounted, uninitialized card with an open worker queue. A second
page states that the operation creates the application dataset without
formatting the card or deleting unrelated files; only its final primary action
submits the asynchronous storage request.
Safe eject, legacy cleanup, card-data deletion and device-binding reset each
have independent eligibility and confirmation. Card deletion never doubles as
device reset, and device reset never mounts or deletes card content.

## Layout ownership

Page-specific geometry lives beside each app. Cross-product launcher geometry
is in `src/ui/app_pages.cpp`; reusable controls and tokens are in
`src/ui/prototype_ui.*` and `src/ui/ui_tokens.h`. When adding a page, update its
renderer and hit testing together and cover both logical orientations in tests.
