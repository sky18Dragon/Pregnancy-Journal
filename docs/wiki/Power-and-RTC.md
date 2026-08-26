# Power and RTC

Sticky Bunny uses deep sleep as an application lifecycle state, not as a blind fixed timer.

## Entering sleep

The deliberate user gesture is holding both non-AI side keys. The firmware first asks the active app whether its current state is safe to preserve, then saves state, draws a clean sleep screen and plays one short confirmation chime.

The AI key remains dedicated to the launcher and global pet return. App-specific idle policies may also request sleep when a stable page has been inactive long enough.

## App preparation

| App | Saved state before sleep |
| --- | --- |
| Desktop pet | Pet save, current epoch and next scheduled event |
| Pomodoro | Setup page or a restorable paused timer state |
| Status Board | Stable selected status page in RTC memory |
| Book of Answers | Stable home or result page; active shake/reveal blocks sleep |

## Event-aware wake-up

Before the pet sleeps, its scheduler returns the next planned autonomous event in RTC seconds. For example, if an outing is arranged for 15:00, the RTC alarm can wake the device shortly before that event rather than polling at a fixed interval. On wake, saved time and current RTC time determine whether to show preparation, the away state, a return, or the normal home page.

## Pet autonomous policy

The pet can remain asleep while its state still advances. The firmware schedules meaningful events, applies offline time after wake, and avoids periodic sound or refresh activity on a new, unhatched egg. Hunger and zero-energy alerts are edge-triggered: entering the condition may sound once, while remaining in the condition stays quiet.

## Sleep screen

The screen receives a clean baseline before the moon indicator is drawn. This removes previous-page ghosting instead of layering the sleep icon over retained content.

Battery percentage moves left to reserve the upper-right corner for the moon. Both values remain visible.

## RTC validity

The PCF8563 provides calendar time and alarms. A low-voltage flag marks the reading invalid. Startup continues with application time as a fallback and retries the RTC later; the pet never blocks at boot because the clock battery was uninitialized.

## Energy recovery

Pet sleep restores energy at 6% per minute. The sleep page states this rate directly. Any recovered value above zero permits interaction after wake; reaching 100% is not required.
