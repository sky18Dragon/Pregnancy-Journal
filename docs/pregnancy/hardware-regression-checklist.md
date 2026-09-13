# Hardware regression checklist

Run this checklist on a Seeed Studio reTerminal Sticky after flashing the final
release image. The previous seven-app image passed a cold-boot smoke test on
2026-09-12. The four-app scope revision built on 2026-09-13 still requires a
fresh physical-device run, so its checks remain open below.

## Boot and devices

- [ ] Cold boot reaches Baby Week without panic or watchdog reset.
- [ ] RTC, battery gauge, SSD1677, GT911, IMU, buttons and buzzer initialize.
- [ ] Existing date/time survives power cycle.
- [ ] English and Simplified Chinese glyphs are legible.

## Apps and input

- [ ] Swipe up and top-button single click open Launcher.
- [ ] Swipe down closes Launcher; double-click returns to Baby Week.
- [ ] All four cards open the correct app.
- [ ] Inactive apps do not receive touch presses.
- [ ] Pregnancy due-date and LMP setup both save and restore.
- [ ] Overview/Baby/Mom tabs render the expected week.
- [ ] Reminder add/list/complete/delete survives reboot.
- [ ] Checkup add/complete/delete survives reboot.

## Display

- [ ] Full and partial refresh complete without timeout.
- [ ] Unchanged areas remain stable during partial refresh.
- [ ] Repeated app changes trigger periodic cleanup and acceptable ghosting.
- [ ] Settings clean refresh removes residual ghosting.

## Sleep and wake

- [ ] Idle timeout enters deep sleep and display image remains visible.
- [ ] Top button wakes the device.
- [ ] Timer wakes near a reminder and starts the buzzer.
- [ ] Touch/button interaction stops the buzzer.
- [ ] Earliest reminder wins over later reminder/checkup/daily refresh.
- [ ] With no earlier event, daily 03:00 refresh is selected.
- [ ] Charge/no-charge behavior and retained GPIO levels match baseline.

## Fault and power-cycle cases

- [ ] Interrupt profile, reminder and checkup writes; valid prior data
  remains loadable or a damaged record is skipped.
- [ ] Full reminder/checkup capacity fails gracefully.
- [ ] RTC unavailable and battery unavailable states remain navigable.
