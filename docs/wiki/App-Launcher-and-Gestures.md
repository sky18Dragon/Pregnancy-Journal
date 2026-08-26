# App Launcher and Gestures

The launcher is a temporary selection layer shared by all applications. Opening it starts a fresh IMU session immediately; closing or selecting an app stops that session.

<p align="center">
  <img src="../images/launcher/launcher-layouts.png" alt="Launcher in portrait and landscape" width="760">
</p>

## Open and close

| Input | Result |
| --- | --- |
| Single AI-key press | Open the launcher; press again to close it |
| Upward swipe starting near the lower edge | Open the launcher |
| Downward swipe starting in the upper half | Close the launcher |
| Double AI-key press | Return directly to the desktop pet from any app page |

Swipe recognition checks the start region, minimum vertical travel, dominant direction, horizontal drift and maximum gesture duration. The close region is intentionally generous, allowing a natural pull from the top toward the middle.

## Touch selection

Every sticker illustration and its label belong to one app touch target. The selection outline follows the active app; the pet card is not permanently highlighted. Portrait uses a two-by-two arrangement and landscape uses a centered row of four.

## Rotation selection

Rotation is available at the same time as touch selection:

- a landscape baseline followed by a stable portrait placement selects Pomodoro;
- a portrait baseline followed by a stable landscape placement selects Status Board.

The IMU begins before the ePaper launcher refresh. A baseline is captured from the first useful upright sample. The fast path accepts a new orientation after five stable samples when the motion window is quiet. The settled path remains available after a longer movement.

The app display rotation is derived from the final physical placement, including both 180-degree variants. This keeps the selected page upright instead of assuming one fixed portrait or landscape direction.

## Shake selection

A continuous launcher shake lasting at least 800 ms selects Book of Answers. Once qualified shaking begins, shake routing has priority over transient orientation changes caused by the same hand motion.

The launcher marks that gesture as consumed. Book of Answers then waits for quiet and a fresh shake before beginning the three-second answer ritual.

## Routing state

The pure router in `src/app/sticky_app_router.*` stores only whether the launcher is open and its baseline orientation. It returns `BaselineCaptured` or `AppSelected`; the app manager owns IMU start/stop, application lifecycle and display refresh.
