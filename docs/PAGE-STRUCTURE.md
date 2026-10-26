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
| Top button single click | Any page → Baby Week home → Launcher → close (back to the page you were on). On the Kicks counter page it counts one movement instead. Setup wizard pages ignore it |
| Top button double click | The shortcut chosen in Settings → Shortcut: today's weight entry (default), Baby Week home, start Kicks, or off. Pages holding unsaved input or a running count are left alone |
| Side button single press | Previous / next app in Launcher order, wrapping; presses within 400 ms merge into one switch. Pages with unsaved input or a running count ignore it |
| Bottom-edge upward swipe | Open Launcher |
| Downward swipe beginning in lower 30% | Close Launcher |
| Bottom-edge swipe left / right (Launcher closed) | Switch to the next / previous app in Launcher order (Baby Week → Weight → Kicks → Settings, wrapping); a bar along the bottom edge acknowledges the swipe |
| Launcher card tap | Pause current app and activate selected app |
| Two side buttons held for two seconds | Request deep sleep |

Each page declares how the keys act on it (Home, Free, Pager, Capture, Locked or
Wizard; `src/ui/key_policy.h`). The design and rationale are in
[key and shortcut design](pregnancy/key-shortcuts-design.md). Launcher inactivity closes it after 30 seconds. Stable app pages normally sleep
after 60 seconds on battery; Settings uses three minutes. Editors and an active
kick session can veto timeout sleep.

## Baby Week

| Page | Purpose | Primary transitions |
| --- | --- | --- |
| Clock setup | First-use RTC entry (`YYYYMMDDHHMM`) | Continue -> source setup |
| Source setup | Choose confirmed due date or LMP | Choice -> date setup; optional Back |
| Date setup | Enter `YYYYMMDD` and validate profile | Save -> Dashboard; Back |
| Dashboard | Engraving-style plate for the week (weeks 6–40) with its English object name, an 8×5 week grid (filled = past, ring with dot = current, empty = ahead) and the large week number with `WEEKS`; no progress bar or countdown | Illustration → Baby; week number block → Mom; menu icon (top right) → Launcher. The week grid is display only |
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
| Counter | Current mode and period, elapsed time and count (n/10 for Count to 10); the top button also counts a movement here | Count, Undo, Finish, Cancel |
| Feel check-in | As usual, weaker, fewer, pattern changed | Choose (saves), Back |
| Result | Saved record against its seven-day median | Done, Compare, Back |
| Compare | Today per period, latest Count to 10, seven-day check-ins | Start, Back |

The Counter remains awake, filters taps closer than 400 ms and auto-finishes at
60 minutes. The UI reports recorded observations and always avoids diagnostic
classification.

## Settings

| Page | Purpose | Primary transitions |
| --- | --- | --- |
| Main | Language, RTC, pregnancy summary, double-click shortcut, display cleanup, MicroSD | Open editor/summary/shortcut/storage; APPS header action (Launcher) |
| Shortcut | Pick the double-click target: today's weight entry, Baby Week home, start Kicks, or off; the current one is inverted | Tap to save and stay; Back to Main |
| Time editor | Edit and validate `YYYYMMDDHHMM` | Save or Back |
| Pregnancy summary | Source, due date, week and progress | Edit or Back |
| MicroSD summary | Read-only card, access and data status plus free space | Initialize/eject, More or Back |
| More options | Old-data import (profile/weight/settings, Kicks), delete card data; unavailable rows are marked and not tappable; dataset identity as a footer | Open the import report or a confirmation, or Back |
| MicroSD confirmation | Explain the exact `PREGJNL` creation boundary | Create dataset, Cancel or Back |
| Old device data | Import report for the selected NVS source | Scan/import, clear verified copies or Back (to More options) |
| Old-copy cleanup confirmation | Name the exact legacy cleanup boundary | Clear verified copies, Cancel or Back |
| Card-data delete confirmation | Delete only the current card's `PREGJNL` directory | Delete app data, Cancel or Back (to More options) |

Settings can request a return to Baby Week but does not switch the registry
itself. The time editor disables timeout sleep to protect in-progress input.
MicroSD initialization is never automatic. The summary enables its action only
for a writable, mounted, uninitialized card with an open worker queue. A second
page states that the operation creates the application dataset without
formatting the card or deleting unrelated files; only its final primary action
submits the asynchronous storage request.

Baby Week, Weight and Kicks share `repository_status_page`. When a read fails
because the dataset is unavailable, the page offers **SET UP MEMORY CARD**
only if the current card is mounted, writable, positively identified as
uninitialized, and the manifest worker accepts requests. Loading/saving,
missing, read-only, full, corrupt, unknown or initialized cards do not offer
this action. The secondary reload and header back actions remain available.
Text wraps within the logical canvas in both orientations and languages.

The setup action posts the displayed mount generation to the coordinator,
which opens Settings directly at the existing MicroSD confirmation. Opening
it creates no files. Settings rechecks eligibility and the same mount
generation on entry and final confirmation; changing cards requires starting
the confirmation again. Cancel/Back return to MicroSD summary without writing.
The original Settings → MicroSD entry remains available.

Safe eject, legacy cleanup, card-data deletion and device-binding reset each
have independent eligibility and confirmation. Card deletion never doubles as
device reset, and device reset never mounts or deletes card content.

## Layout ownership

Page-specific geometry lives beside each app. Cross-product launcher geometry
is in `src/ui/app_pages.cpp`; reusable controls and tokens are in
`src/ui/prototype_ui.*` and `src/ui/ui_tokens.h`. When adding a page, update its
renderer and hit testing together and cover both logical orientations in tests.
