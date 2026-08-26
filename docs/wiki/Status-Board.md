# Status Board

Sticky Status Board turns the device into a landscape availability sign.

<p align="center">
  <img src="../images/status-board/menu.png" alt="Status Board menu" width="760">
</p>

## Preset states

- `BUSY`
- `MEETING`
- `ON CALL`
- `OPEN TO TALK`
- `REST`
- `CUSTOM`

The selected menu card uses a dark treatment. A pet animation moves along the lower baseline without occupying the entire unused area.

## Full-screen display

Selecting a state opens a landscape display with large text and a state-specific rabbit scene. Time is not shown because the firmware does not invent an end time for the user.

<p align="center">
  <img src="../images/status-board/open-to-talk.png" alt="Open to Talk status" width="760">
</p>

The visible back arrow is intentionally small while its touch target extends around it. This keeps the status clean and makes returning reliable.

## Custom status

The device includes its own QWERTY and numeric keyboard. A custom message may contain up to 20 characters. `DELETE` removes one character, `CLEAR` clears the complete field, and `APPLY` opens the full-screen state. Empty input is rejected with an inline message.

<p align="center">
  <img src="../images/status-board/custom.png" alt="Custom landscape status" width="760">
</p>

Touch actions received during an ePaper update are batched and processed after the current refresh, keeping the keyboard and back action responsive.
