# Troubleshooting

Start with the symptom that matches the device and follow the checks in order.

## PlatformIO cannot find or connect to the device

1. Use a USB-C cable known to carry data.
2. Connect directly to the computer or a powered, stable hub.
3. List ports with `pio device list`.
4. Put Sticky into its download/reset sequence and retry the upload.
5. Reduce upload baud if the connection reports serial noise or a stopped data stream.

Successful recovery ends with esptool writing the image and PlatformIO reporting `SUCCESS`.

## The display stays blank

Open a debug serial monitor and confirm this startup order:

- `board_shared_spi` prepares the SD control pins;
- display initialization completes;
- the physical clear reports success;
- touch initialization starts after display ownership is established.

A missing shared-SPI preparation can prevent the ePaper panel from receiving a clean transaction.

## The screen shows an older image or ghosting

A fresh cold boot performs a physical white clear. For sleep pages and app changes, confirm the log reports the intended full or quality refresh. Rebuilding only the application image retains NVS but does not explain old physical ink; the panel must receive a real clear operation.

## Touch produces no event

The debug log should show:

```text
GT911 ... id='911'
sticky_touch: touch=controller_ready address=0x14
sticky_touch: touch=polling_ready
```

If controller identity is missing, inspect GPIO 42 enable, GPIO 41 reset, GPIO 21 interrupt and the GPIO 2/3 touch I2C bus. If identity is correct, enable accepted touch logs and compare coordinates at the four corners.

## Touch feels delayed during an ePaper refresh

The input queue keeps the touch, but the active application applies it after the current panel transaction finishes. Compare `queue_latency_ms` with the display refresh `elapsed_ms`. A large queue delay paired with a normal touch event indicates refresh policy or page size, not a missing touch.

## Rotation does not select an app

Open the launcher first and confirm IMU monitoring begins immediately. Hold the initial orientation long enough to establish a baseline, rotate through 90 degrees, then place the device steadily in the final orientation. The log should show baseline capture followed by an app selection.

## Shaking opens Answers but immediately starts a result

The launcher must mark its shake as consumed. Book of Answers should report that it is waiting for quiet, become ready, and only then accept a fresh three-second shake.

## RTC reports low voltage

`validity=low_voltage` means the calendar reading is not yet trusted. The firmware continues with application time and retries later. Set a valid RTC time or keep the board powered long enough for the clock supply to stabilize, then confirm a complete timestamp appears at the next read.

## The device works on USB but stops on battery

Confirm board power hold/lock initialization and the charging path before display startup. Check battery percentage from the BQ27220 and test after a full charge. A normal battery boot reaches `phase=ready` without an external-power requirement.

## The buzzer sounds repeatedly without interaction

Pet hunger and zero-energy notifications are transition-based. The first entry into the condition may sound once; staying there remains quiet. Recovering above the threshold and entering it again starts a new single notification cycle.
