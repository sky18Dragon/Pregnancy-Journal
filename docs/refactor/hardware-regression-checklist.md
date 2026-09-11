# Hardware regression checklist

Run on a Seeed Studio reTerminal Sticky after flashing `sticky-release`.

- [ ] Boot reaches `phase=ready`; Home shows time, date, battery and RTC state.
- [ ] Swipe up from the bottom edge opens a three-card launcher.
- [ ] Home, Pregnancy and Settings cards select and return cleanly.
- [ ] Double-click returns Home from Settings and Pregnancy.
- [ ] Settings language toggle changes both launcher and page copy between English and 简体中文.
- [ ] Settings time editor writes a valid PCF8563 time and rejects invalid input.
- [ ] Settings clean-refresh action clears accumulated e-paper ghosting.
- [ ] Pregnancy first use accepts device time and due date, then dashboard updates week/day/progress.
- [ ] Battery overlay still reports charge and external-power state.
- [ ] Side-button sleep chord performs a final full refresh and deep sleep.
- [ ] Wake from sleep restores the valid app context or safely falls back to Home.
- [ ] No touch, RTC, I2C, SPI, display or task watchdog errors appear in the serial log.

Record the firmware commit, port, reset/wake reason and any failed item in the
test log before changing the hardware again.
