# Pomodoro Timer

Sticky Pomodoro Timer is a portrait ePaper timer designed around a large, stable time display.

<p align="center">
  <img src="../images/pomodoro/setup.png" alt="Pomodoro setup with 25 minutes selected" width="330">
</p>

## Select a duration

The main page keeps three presets on one row:

- `15 MIN`
- `25 MIN`
- `60 MIN`

`START` begins the selected session. `CUSTOM TIME` opens an on-device numeric editor for hours, minutes and seconds.

## Custom time editor

<p align="center">
  <img src="../images/pomodoro/custom-time.png" alt="Custom Pomodoro time editor" width="330">
</p>

- Tap a field, then enter digits.
- `DELETE` removes the latest digit from that field (`23` becomes `20`).
- `CLEAR` resets the selected field to `00`.
- `USE THIS TIME` accepts a valid non-zero duration.
- `BACK` returns to setup with a deliberately large touch region.

Minutes and seconds from 60 to 99 are rejected. A fully zero duration remains on the editor page.

## Countdown

<p align="center">
  <img src="../images/pomodoro/running.png" alt="Running Pomodoro countdown" width="330">
</p>

The remaining time updates every second for every preset and custom duration. The display policy refreshes the time region instead of repainting the entire screen each second. Touch events remain queued while an ePaper update completes, so `PAUSE` and `END SESSION` do not become unavailable during a long countdown.

`PAUSE` freezes elapsed time and becomes `RESUME`. `END SESSION` overlays a compact confirmation dialog on the current countdown. `CANCEL` immediately continues the timer; `END` returns to setup while retaining the selected duration.

## Time's up

<p align="center">
  <img src="../images/pomodoro/alarm.png" alt="Pomodoro alarm page" width="330">
</p>

The alarm page uses the same clock size and center as the countdown. A gentle rising three-note pattern repeats with quiet gaps until the user taps `END`. Returning to setup preserves the completed duration.
