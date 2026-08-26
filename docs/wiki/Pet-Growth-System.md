# Pet Growth System

Sticky Bunny combines long-term growth with daily care. Growth points unlock a stage only when the rabbit is also in a healthy enough condition to evolve.

<p align="center">
  <img src="../images/desktop-pet/growth-lineage.png" alt="Pet growth lineage" width="650">
</p>

## Life stages

| Stage | Growth range | Visible change |
| --- | ---: | --- |
| Egg | Before hatch | Nest, cracks and hatch progress |
| Hatchling | 0–29 | Small seated rabbit and first care loop |
| Child | 30–119 | Taller body, longer ears and scarf |
| Youth | 120–279 | One of three personality designs |
| Adult | 280 | Final body proportions, actions and dialogue |

Stage thresholds are evaluated together with care state. Reaching a number while severely hungry or exhausted pauses evolution until the rabbit receives the required care.

## Personality evidence

Care during childhood records three independent tendencies:

- feeding contributes `FOODIE` evidence;
- petting contributes `AFFECTIONATE` evidence;
- playing contributes `ACTIVE` evidence.

A clear leading tendency selects the youth branch automatically. A close result opens an on-device choice page:

<p align="center">
  <img src="../images/desktop-pet/personality-choice.png" alt="Three-way personality choice" width="330">
</p>

Personality changes artwork, autonomous behavior, dialogue tone, interaction poses, sounds and the keepsake brought home after an outing.

## Love and dialogue

Love is bounded from 0 to 100. Dialogue is selected by stage, personality, mood and bond level. A recent-phrase ring keeps the last lines out of the immediate selection pool, so repeated taps do not cycle through the same few messages.

## Fullness, energy and mood

Fullness decays with elapsed pet time. Energy is consumed by the daily model and restored through sleep at 6% per minute. Mood is derived from the current need values rather than stored as an unrelated number, keeping the visible label consistent after reboot or offline progression.

## Daily care and milestones

The first qualifying care action on a new RTC day extends the care streak. Further actions on the same day still affect the pet but do not increment the streak again. Milestones at 3, 7, 30 and 100 days trigger once and are saved so a restart cannot replay the same celebration.

## Offline progression

The save records the last trusted RTC time. At boot, elapsed minutes are applied in bounded steps to fullness, energy, day transitions, scheduled events and return greetings. A low-voltage or invalid RTC result falls back to application time without blocking startup.

## Persistent save

Two versioned NVS records alternate between slot A and slot B. Each record contains a sequence number and checksum. Loading selects the newest valid record; if it is damaged, the other valid slot remains available. Save records include identity, needs, progression, recent history, outings, milestones and timing metadata.

The detailed balancing rationale and source audit remain in [`docs/desktop_pet_growth_system.md`](../desktop_pet_growth_system.md).
