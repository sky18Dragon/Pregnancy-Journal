# Hardware regression checklist

Run this checklist on a Seeed Studio reTerminal Sticky after flashing the
current `sticky-release` image. Host tests and a successful build do not replace
these physical checks. Record the date, firmware commit and serial port with
the result.

## Boot and device services

- [ ] Cold boot reaches `phase=ready` without panic or watchdog reset.
- [ ] RTC, battery gauge, SSD1677 e-paper, GT911 touch, IMU, buttons and buzzer
  initialize in the serial log.
- [ ] Device time survives a power cycle and invalid time input is rejected.
- [ ] English and Simplified Chinese glyphs are legible; launcher language
  toggle updates the visible copy.

## Launcher and navigation

- [ ] Top-button single click opens and closes Launcher.
- [ ] Bottom-edge upward swipe opens it; downward swipe from the lower 30%
  closes it.
- [ ] Double-click returns to Baby Week from every app.
- [ ] Baby Week, Checkups, Reminders, Weight, Kicks and Settings each open the
  correct app.
- [ ] Launcher closes after inactivity and inactive apps do not receive touch.
- [ ] Selecting an app persists the last app across a normal deep-sleep wake.

## Baby Week

- [ ] First use accepts `YYYYMMDDHHMM` device time.
- [ ] Confirmed due date setup saves and restores.
- [ ] LMP setup saves and restores.
- [ ] Dashboard shows week/day, trimester, due-date countdown and 40-week
  progress; overdue dates remain readable.
- [ ] Baby and Mom tabs show the expected offline content for the current week.

## Reminders and checkups

- [ ] Reminder Today/Upcoming views and all six types render correctly.
- [ ] Add, complete, delete and global enable/disable survive a reboot.
- [ ] A full reminder list fails gracefully at 12 records.
- [ ] Checkup add defaults to today +7 days at 09:00; −1/+1/+7 day controls work.
- [ ] Checkup complete/delete survive a reboot and capacity 8 fails gracefully.

## Weight and kicks

- [ ] Weight add/edit saves one value for today; saving again replaces today's
  value.
- [ ] kg/lb toggle, height adjustment (100–220 cm), BMI, baseline and trend
  render correctly; remove-latest removes the chronological latest record.
- [ ] Weight capacity 32 fails gracefully.
- [ ] Kick session infers morning/afternoon/evening from RTC and counts taps in
  the central area.
- [ ] Taps within five seconds are ignored; Finish saves and Cancel discards.
- [ ] An active session ends at 60 minutes; Reset Today removes today's sessions.
- [ ] Three-period summary, 12-hour estimate and attention prompt render.

## Display and power

- [ ] Full and partial refresh complete without timeout.
- [ ] Repeated app changes trigger cleanup refresh without unacceptable ghosting.
- [ ] Settings clean refresh removes residual ghosting.
- [ ] Stable pages enter deep sleep after 60 seconds without external power;
  Settings uses three minutes and editors/active kick counting stay awake.
- [ ] Top-button wake restores the app and the e-paper image remains visible.
- [ ] A two-second side-button chord enters sleep; charge/no-charge GPIO levels
  match the hardware baseline.

## Scheduled wake and fault cases

- [ ] Earliest reminder/checkup wins over the daily 03:00 refresh and app event.
- [ ] Timer wake up to 15 seconds early starts the buzzer for a due item.
- [ ] Touch/button interaction stops the buzzer.
- [ ] RTC or battery unavailable states remain navigable.
- [ ] Interrupted/corrupt NVS writes preserve valid prior records or skip only
  the damaged slot.

## Verification record

- Firmware: `2.0.0`
- Current repository commit: `2556df8`
- Host suite: 20/20 passed
- Current six-app physical run: pending until the checklist above is completed
