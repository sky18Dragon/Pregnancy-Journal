# Desktop Pet Growth System

## Product Goal

The desktop pet is a long-term companion that grows through daily care. A user can feed, pet, and play with the rabbit in a short daily session. Those choices build growth progress, affection, and one of three personality paths.

The first release follows one shared childhood and three personality branches:

```text
Egg -> Hatchling -> Child -> Foodie Youth       -> Foodie Adult
                         -> Affectionate Youth  -> Affectionate Adult
                         -> Active Youth        -> Active Adult
```

The current character reference is stored at:

`assets/desktop_pet/concepts/growth_lineage_v1.png`

## Core Player Loop

The daily experience is designed to take about 30 to 90 seconds:

1. Open the pet page and receive a stage- and affection-aware greeting.
2. Feed, pet, or play with the rabbit.
3. Watch a short e-paper-friendly action animation.
4. Gain growth and affection within the daily reward limits.
5. Return on later days to unlock a new life stage, dialogue, and animation set.

Every interaction remains available after its daily reward has been collected. Later interactions continue to produce animation and dialogue, while the reward counters remain stable until the next day.

## Persistent Values

The saved state contains these groups of data:

### Identity

- Save-data version
- User-chosen name: up to 10 uppercase letters, digits, or spaces
- Life stage
- Personality branch
- Hatch date
- Total companion days

### Progress

- Growth points: `0..280`
- Affection: `0..100`
- Current care streak
- Longest care streak

### Daily Care

- Current date key
- Growth earned today
- Affection earned today
- Rewarded feeds today
- Rewarded petting sessions today
- Rewarded play sessions today
- First-visit reward status

### Personality Evidence

- Foodie path score
- Affectionate path score
- Active path score
- The most recent rewarded care action

### Dialogue Memory

- Recently shown dialogue identifiers
- Last greeting context
- Last absence length

All values are stored in a versioned NVS record. A state migration function converts older records when new fields are introduced.

## Life Stages

| Stage | Growth range | Expected care time | Visible change |
| --- | ---: | ---: | --- |
| Egg | Before hatching | First session | Egg rocks and cracks after three touches |
| Hatchling | 0-29 | About 3 active days | Round body, short ears, oversized feet |
| Child | 30-119 | About 9 additional active days | Longer ears, stable walking and clearer expressions |
| Youth | 120-279 | About 16 additional active days | Personality branch becomes visually distinct |
| Adult | 280 | Around the fourth week | Complete branch silhouette and exclusive actions |

Growth points preserve the user's achieved progress. Each stage transition plays once, saves immediately, and then opens the new stage home scene.

## Daily Rewards

The production balance uses a daily growth cap of `10` and a daily affection cap of `8`.

| Interaction | Rewarded uses per day | Growth | Affection | Personality evidence |
| --- | ---: | ---: | ---: | ---: |
| First visit | 1 | +1 | +1 | None |
| First meal | 1 | +4 | +1 | Foodie +3 |
| Snack | 1 | +2 | +1 | Foodie +3 |
| Petting | 3 | +1 each | +2 each | Affectionate +2 each |
| Play | 1 | +3 | +3 | Active +6 |

The evidence weights give each path the same maximum daily contribution:

- Foodie: `6` points per day
- Affectionate: `6` points per day
- Active: `6` points per day

This keeps the route comparison fair even though the three actions have different daily use counts.

When one action reaches its daily reward count, it switches to a companion response:

- Extra food produces a full and satisfied response.
- Extra petting produces a relaxed social response.
- Extra play produces a short free-play response.

These responses keep the pet interactive without changing daily progress.

## Balance Profiles

The growth engine reads all timing and reward values from one balance profile. The test firmware and production firmware use the same state machine, stage thresholds, action limits, personality evidence, persistence format, and dialogue rules.

### Fast Test Profile

The first desktop-pet development build will use `STICKY_DESKTOP_PET_TEST_MODE=1` with these values:

| Setting | Test value |
| --- | ---: |
| Simulated day length | 120 seconds |
| Growth reward multiplier | 10x |
| Affection reward multiplier | 5x |
| Daily growth cap | 100 |
| Daily affection cap | 40 |
| Personality evidence multiplier | 1x |
| Stage thresholds | 30 / 120 / 280 |

With full daily care, the test profile reaches:

- Child during the first simulated day
- Youth during the second simulated day
- Adult during the third simulated day
- Close affection during the first simulated day
- Best Friend affection during the second or third simulated day

A complete lifecycle can therefore be exercised in about six minutes while retaining real daily rollover, reward caps, route scoring, stage transitions, persistence, and dialogue changes.

The test home page will display a small `TEST` badge. Tapping it opens a development panel with:

- `NEXT DAY`: advances exactly one simulated date and runs normal rollover logic
- `+30 GROWTH`: moves toward the next threshold through the normal stage evaluator
- `+20 LOVE`: moves through affection dialogue tiers
- `GO OUT`: plays one accelerated 20-40 second outing and supports calling the rabbit home from its note
- `RESET PET`: clears only desktop-pet test state after confirmation
- Current stage, growth, affection, and three personality scores

The accelerated rewards will still pass through the normal daily counters and caps. Personality evidence keeps its production values so automatic and close-score branch decisions remain representative.

### Current Implemented Slice

The current visible firmware implements Egg, Hatchling, Child, all three Youth personality forms, and all three matching Adult forms. A new record opens on a full-screen Egg page. Three completed taps are saved independently: the first tap rocks and cracks the Egg, the second reveals two ears, and the third opens the shell, plays a gentle three-note chime, and creates the Hatchling baseline at growth `10`, affection `18`, and food `80`. A Hatchling with `30` growth and all four care needs at or above `40` automatically plays a three-frame full-page evolution scene and opens the Child home. At `120` growth, the pet either follows a clear score leader or opens the final three-way care choice before saving and revealing the selected Youth form. At `280` growth, a well-cared-for Youth keeps its selected branch and plays a second three-frame transition into the corresponding Adult form.

An independent core framework now exists under `src/apps/desktop_pet/core/`. It models six life stages, food, joy, energy, hygiene, seven relationship moods plus urgent need states, sleep, waste, care mistakes, bond, streaks, personality evidence, evolution readiness, dialogue history, bounded offline progression, a fixed animation queue, RTC conversion, and validated two-slot save records.

The approved Egg and Hatchling UI stores the user-chosen name, hatching progress, growth, bond, daily counters, personality evidence, food, energy, and mood in the versioned desktop-pet record. After the final hatching frame, a portrait QWERTY keyboard collects a name of up to 10 characters. The home page displays the saved name and opens the same editor when the name or stage heading is tapped. Food and energy are visible together; play consumes energy, values from `1` through `9` select the low-energy state, keep the tired pose between interactions, and pause autonomous actions while preserving all manual interactions. Zero energy immediately replaces any current action with the prone frame and pauses manual actions until sleep begins. Tapping the zero-energy pet starts sleep. The sleep page alternates two complete moonlit-room frames, restores energy through the existing core simulation, accepts an early wake touch, and returns home automatically at full energy. Any positive energy immediately restores normal interactions after an early wake. Version-2 through version-5 pet saves migrate into the current record and preserve their achieved stages without interrupting startup; an existing unnamed pet can be named from the home-page heading.

The interaction state now emits a semantic pet performance such as eating, receiving petting, playing, speaking, falling asleep, or waking. `desktop_pet_sound_cues` maps that visible performance together with the current mood, bond tier, life stage, personality branch, and dialogue into a short passive-buzzer pattern. The application refreshes the matching rabbit frame before starting the background sound. Hungry, tired, and sad expressions take priority over normal age- and personality-based speech. Routine stretch, blink, ear-twitch, and look-around frames remain silent.

The first outing slice is available from the accelerated test panel. `desktop_pet_outing` advances the rabbit through packing, leaving, away, returning, reunion, and home without blocking touch polling. The away page keeps the furnished room visible, replaces the shelf with an open doorway, leaves footprints and a tappable note, and alternates a small room detail. Calling the rabbit home from the note enters the same return sequence as a completed outing. Personality routes return with distinct berry, flower, or leaf souvenirs. The test duration is uniformly selected from 20 through 40 seconds, while the approved production duration range is one through seven hours. Automatic daytime scheduling and restart-safe elapsed time will use the validated RTC adapter in the runtime integration slice.

Cleaning, PCF8563 hardware time, and the validated two-slot save backend remain independent integration blocks. The current test build accelerates sleep recovery from the app timer; the later PCF8563 adapter will provide real elapsed time without changing the sleep-state rules.

### Production Profile

The final firmware uses `STICKY_DESKTOP_PET_TEST_MODE=0` with these values:

| Setting | Production value |
| --- | ---: |
| Day source | Trusted local calendar date |
| Growth reward multiplier | 1x |
| Affection reward multiplier | 1x |
| Daily growth cap | 10 |
| Daily affection cap | 8 |
| Personality evidence multiplier | 1x |
| Stage thresholds | 30 / 120 / 280 |

Switching profiles changes balance constants and test controls. Saved fields, stage meanings, route meanings, action counts, and content identifiers remain identical.

## Personality Branch Decision

The personality branch is evaluated when growth reaches `120`.

1. Compare the three accumulated personality scores.
2. A route becomes automatic when its score leads the second-place score by at least `6` points.
3. When the top scores are closer than `6`, the rabbit enters a bonding-choice scene.
4. The user completes one final feeding, petting, or play action to choose the route.
5. The selected branch is saved before the youth evolution animation begins.

The score remains internal during childhood. The home page communicates personality through behavior and dialogue instead of showing three competitive meters.

## Affection

| Affection | Relationship | Interaction tone |
| ---: | --- | --- |
| 0-24 | Shy | Short answers, cautious posture, curious glances |
| 25-49 | Familiar | Greets the user and responds more openly |
| 50-79 | Close | Requests attention and shares branch-specific feelings |
| 80-100 | Best Friend | Exclusive greetings, trust animations, and intimate dialogue |

Affection gains follow the daily cap. Calendar catch-up applies a gentle return model:

- The first three missed days form a grace period.
- Each later missed day changes affection by `-2`.
- One startup catch-up changes affection by at most `-10`.
- A return greeting acknowledges the absence and immediately offers a positive care action.

Growth progress remains permanent, so returning users continue from their achieved life stage.

## Care Streak

The first rewarded interaction of a calendar day records one active care day.

- Consecutive active dates increase the current streak.
- A missed date starts a new current streak on the next active day.
- The longest streak remains as a lifetime record.
- Milestones at 3, 7, 14, and 30 days unlock one-time dialogue and small celebratory poses.

The streak supports positive recognition. Growth and core interactions remain available at every streak value.

## Dialogue System

Dialogue is selected from tagged entries. Each entry can specify:

- Context: greeting, idle, feed, pet, play, full, tired, return, evolution ready, or evolution complete
- Minimum and maximum life stage
- Personality branch or shared branch
- Minimum and maximum affection
- Selection weight
- Dialogue identifier
- Display text

The selector uses this order:

1. Filter by the current context.
2. Filter by life stage.
3. Prefer the current personality branch after the youth evolution.
4. Filter by affection range.
5. Remove the five most recently displayed identifiers.
6. Choose from the remaining weighted entries.
7. Use a shared fallback entry when a specialized group is empty.

The first content set targets at least 120 English lines:

- 48 shared stage and affection lines
- 36 branch-personality lines
- 24 interaction-result lines
- 12 milestone, return, and evolution lines

The firmware stores dialogue as data, allowing new lines to be added without changing the growth engine.

## Time and Date Rules

The pet engine receives time through a clock interface.

Production mode uses the device's PCF8563 RTC as the trusted local date source. The hardware adapter validates the calendar fields and passes them to `pet_rtc_time`, which converts the value to a continuous epoch timestamp. The adapter marks the first saved-time catch-up as offline and later running updates as online. The core then performs daily rollover and limits one offline catch-up to fourteen days.

Food is a simulated pet need. Awake time lowers food according to the active balance profile, sleeping lowers it more slowly, and feeding restores it directly. Device battery level remains independent from this loop.

Development mode provides controlled time:

- Roll over automatically every 120 seconds of test runtime.
- Advance one simulated day manually from the test panel.
- Set growth and affection values.
- Force any life stage or personality branch.
- Reset pet data through a dedicated development action.

This mode makes a four-week lifecycle testable in minutes while preserving the production rules.

## E-Paper Animation Rules

Each action uses two to four key frames with clear pose changes:

- Idle: ears, eyes, paws, or breathing silhouette
- Feed: reach, bite, chew, satisfied pose
- Pet: initial touch, ears relax, happy response
- Play: anticipation, large movement, landing, celebration
- Evolution: old silhouette, transition frame, new silhouette

The animation controller redraws only the pet and nearby prop region during an action. A stable final frame remains after the action finishes. Page changes and evolution scenes use a full-page refresh, while ordinary actions use the panel's monochrome fast-refresh path.

The display policy tracks visible partial refreshes and requests a cleanup refresh at a threshold verified on Sticky hardware.

## Core Software Modules

The independent framework currently contains:

- `pet_core`: needs, actions, mood, elapsed-time simulation, streaks, growth, and stage transitions
- `pet_dialogue`: tagged filtering, recent-line protection, and compact firmware text
- `pet_animation_queue`: a fixed 16-node non-blocking action sequence
- `pet_save_record`: versioned records, checksum validation, sequence ordering, and two-slot selection
- `pet_rtc_time`: validated PCF8563 calendar conversion

The existing `desktop_pet_app`, `desktop_pet_pages`, `desktop_pet_state`, and `desktop_pet_storage` modules run the approved Hatchling, Child, and three-route Youth UI and use `PetCoreState` for visible care values. The state wrapper keeps UI actions separate from the reusable rule engine, while storage version 4 migrates both version-2 and version-3 records before preserving the chosen personality branch.

The Hatchling home screen maps direct taps on the rabbit body to petting. The bottom action row contains `FEED`, `TALK`, and `PLAY`. `TALK` selects urgent need dialogue first, then uses bond ranges `0-34`, `35-69`, and `70-100` for increasingly familiar lines. Talking does not award growth or bond points, and its selected line remains visible for four seconds.

The Hatchling also runs a non-blocking autonomous behavior loop. Common actions include blinking, a two-step ear twitch, looking around, and a full-body stretch. Hungry and tired moods add dedicated belly-holding and resting poses. Each action uses the shared fixed animation queue, avoids the two most recent selections, synchronizes its dialogue with the visible pose, and yields immediately when a touch action arrives. Development timing is 4-8 seconds between sequences; production timing is 12-28 seconds.

The Child stage uses a taller long-eared rabbit with a neckerchief and its own complete set of idle, blink, ear-twitch, look-around, stretch, hungry, tired, feed, pet, and play bitmaps. The Youth stage then separates into Foodie, Affectionate, and Active silhouettes, accessories, signature movements, interactions, home titles, and route-specific dialogue.

The framework stays independent from the display, touch controller, IMU, NVS driver, and RTC driver. Native tests can therefore validate pet behavior on a computer. The NVS and PCF8563 hardware adapters remain explicit integration tasks.

## Open-Source Source Library

The audited source archive lives in `assets/desktop_pet/library/`, while exact commits and license texts live in `third_party/virtual_pet/`.

- TamaPoke contributes needs, sleep, bounded offline progression, care mistakes, bond, streak, and user-present evolution concepts.
- esp32-artoria-tamagotchi contributes the fixed animation queue, calendar conversion, versioned record, checksum, and rotating save-slot structure.
- openclaw-tamagotchi contributes three dialogue source books totaling 132 lines; the runtime rabbit subset is normalized into a compact English table.
- ESP32-TamaPetchi contributes mood vocabulary, personality evidence, action-memory, and rest concepts.

The source archive is not compiled into firmware. Only compact tables and rules selected for Sticky are linked, keeping visual bitmaps as the main Flash consumer.

## Logging Requirements

Normal development logs record:

- Daily rollover result
- Rewarded care action
- Daily growth and affection totals
- Stage transition readiness and completion
- Personality score comparison and selected branch
- Persistence load, migration, and save result

High-frequency frame logs use a dedicated compile-time switch named `STICKY_LOG_DESKTOP_PET_ANIMATION_ENABLED`. The default development output remains focused on user actions and lifecycle changes.

## Native Test Matrix

The first implementation keeps regression tests for:

1. First-visit reward occurs once per date.
2. Every action stops adding values at its daily reward count.
3. Total daily growth stops at `10`.
4. Total daily affection stops at `8`.
5. A date rollover resets only daily counters.
6. Growth reaches the correct stage thresholds.
7. Growth remains stable across missed dates.
8. Affection grace and catch-up limits are applied once.
9. Each dominant personality score selects the expected branch.
10. Close personality scores enter the bonding-choice state.
11. The final bonding action selects and persists a branch.
12. Dialogue respects context, stage, branch, and affection tags.
13. Dialogue does not repeat one of the five recent entries when alternatives exist.
14. Save loading validates ranges and migrates older versions.
15. Replaying the same date after reboot produces the same persistent result.
16. Test multipliers still stop at the test-profile daily caps.
17. Test and production profiles produce the same route result from the same care-action sequence.
18. `NEXT DAY` applies one rollover and remains idempotent across an immediate reboot.

## First Implementation Boundary

The implemented vertical slices cover Hatchling, Child, and Youth entry:

- New pet state and egg hatching
- Hatchling home page
- Feed, direct-pet, talk, and play actions
- Growth and affection rewards
- Daily caps and date rollover
- Persistent save and reload
- Two-minute simulated days and visible development controls
- Native rule tests
- Automatic care-gated Hatchling-to-Child evolution
- Three-frame full-page evolution scene
- Child home, progress target, interactions, autonomous actions, and dialogue
- Six-point automatic personality decision and final choice page for close scores
- Foodie, Affectionate, and Active Youth homes, progress targets, actions, and dialogue

The rule framework underneath these slices is implemented and native-tested. Later slices connect the PCF8563 adapter and validated two-slot NVS backend, then add three Adult forms and expanded need-specific Youth animation sets on top of the same core.
