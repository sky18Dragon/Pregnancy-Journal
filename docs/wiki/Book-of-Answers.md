# Book of Answers

The Book of Answers uses the IMU as part of the ritual: choose a mode, hold a question in mind and shake continuously for three seconds.

<p align="center">
  <img src="../images/book-of-answers/home.png" alt="Book of Answers home" width="330">
</p>

## Answer modes

### Message

`MESSAGE` is selected by default. The firmware chooses from 350 short answers and avoids returning the same message twice in a row.

<p align="center">
  <img src="../images/book-of-answers/message-result.png" alt="Message answer result" width="330">
</p>

### Yes or No

The crystal-ball mode returns exactly one of three results:

- `YES`
- `NO`
- `UNCLEAR`

The text is centered inside a detailed ball with a faceted base and rabbit interaction.

<p align="center">
  <img src="../images/book-of-answers/crystal-result.png" alt="Crystal ball YES result" width="330">
</p>

## Shake qualification

A single hard movement is not enough. The IMU records effective peaks across a continuous shake session and requires three seconds of qualified movement. Stopping early opens a concise `SHAKE FOR 3 SECONDS` page, then returns to the selected mode.

After qualification, the device asks the user to hold still. Thinking and revealing animations finish normally before the result appears.

## Launcher entry guard

Shaking in the app launcher may select the Book of Answers, but that same gesture is consumed by the launcher. The app waits for the device to become quiet and requires a fresh shake before beginning an answer. This creates a clear home-screen pause instead of flowing directly into a result.

`ASK AGAIN` returns to the current mode. `END` uses a larger touch target than its visible outline and returns to the home page.
