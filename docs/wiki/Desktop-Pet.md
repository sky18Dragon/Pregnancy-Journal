# Desktop Pet

The rabbit is the default home screen and continues living even when another app is open or the ESP32-S3 is sleeping.

<p align="center">
  <img src="../images/desktop-pet/home.png" alt="Desktop pet home screen" width="360">
</p>

## Home values

| Value | Meaning |
| --- | --- |
| `GROWTH` | Long-term progress toward the next stage. Care and time both matter. |
| `LOVE` | Bond with the user, from 0 to 100. It changes dialogue and reactions. |
| `FULLNESS` | Hunger state. Feeding restores it; passing days reduces it. |
| `ENERGY` | Available energy for interaction. Sleep restores 6% per minute. |
| `DAY` and date | The RTC-backed pet day and current calendar date. |

## Main interactions

- **Feed** — tap the bowl. Feeding restores fullness and contributes food-oriented personality evidence.
- **Pet** — tap the rabbit itself. The large body hit area makes affection direct and contributes affectionate evidence.
- **Talk** — tap the speech icon. Dialogue depends on life stage, personality, love, mood, fullness, energy and recent phrases.
- **Play** — tap the ball. Play contributes active personality evidence and uses stage-specific motion.
- **Rest** — tap the underlined energy label. The sleep page shows the 6% per minute recovery rule and allows an early wake-up.

Every successful interaction displays the rabbit's response first. Sound follows the visible action, so the user is interacting with the pet rather than with a button sound.

## Egg and naming

<p align="center">
  <img src="../images/desktop-pet/egg.png" alt="Pet egg waiting to hatch" width="330">
</p>

Three deliberate taps advance the egg through two cracks and a hatch reveal. The naming keyboard appears only after hatching. Letters and digits are entered on the device; the name is stored with the pet save.

## Autonomous life

While the pet page is visible, the rabbit can blink, twitch an ear, look around, stretch, show hunger or tiredness, and use stage-specific idle actions. Recent-action memory prevents the same short loop from repeating continuously.

The rabbit may also schedule an outing on some days. It packs, walks to the right, leaves footprints, stays away for a production-time duration between one and seven hours, then returns with a personality-specific keepsake. `CALL HER HOME` ends the outing early.

<p align="center">
  <img src="../images/desktop-pet/outing.png" alt="Empty room while the rabbit is outside" width="330">
</p>

## Low energy

Below 10% energy, the rabbit rests on the floor and random active animations stop. At 0%, feed, talk and play remain unavailable until the pet has slept long enough to recover. The rabbit can wake and interact as soon as energy is greater than zero; a full 100% charge is not required.

## Guide access

The home screen includes a small illustrated book entry. Tapping it reopens the complete firmware tutorial without resetting the pet.

Continue with [Pet Growth System](Pet-Growth-System.md) for the rules behind stages, personality, time and persistence.
