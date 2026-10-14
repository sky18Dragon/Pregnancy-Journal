# E-paper product design system

This file is the top-level visual and interaction contract for Pregnancy
Journal. Detailed historic analysis remains under `design/` and `docs/`, while
this document records the rules that current code should preserve.

## Design intent

The product should feel calm, legible and dependable on a small always-visible
e-paper surface. The hierarchy is editorial rather than app-like: one dominant
fact, a small set of supporting facts, obvious actions and no decorative motion.
Pregnancy information is presented as personal record-keeping and preparation,
never as a clinical verdict.

## Display constraints

- Physical panel: 3.97-inch, 800×480, four-level grayscale e-paper.
- Application canvas: 800×480 landscape or 480×800 portrait, top-left origin.
- Current production pages are transported primarily as 1-bit black/white.
- E-paper retains the last frame without power, refreshes visibly and may ghost
  after repeated fast updates. Design changes must account for those traits.
- Avoid animation, hover states, rapid polling, thin low-contrast detail and
  dense controls that assume LCD-like refresh.

## Foundations

The canonical tokens are in `src/ui/ui_tokens.h`:

| Role | Value |
| --- | ---: |
| Safe/content margin | 24 px |
| Minimum touch height | 48 px |
| Preferred/button height | 56 px |
| Body text | 18 px |
| Section heading | 24 px |
| Page heading | 28 px |
| Display number | 52–56 px |
| Rule/border | 1 px |
| Emphasis stroke | 3 px |

Use integer-aligned geometry. Primary text and outlines are black on white.
Gray is secondary and should not carry the only indication of selection,
urgency or disabled state. Prefer whitespace, rules, stroke weight and fill
inversion over ornamental color.

## Layout

- Landscape may use two columns; portrait must reflow into a vertical sequence
  rather than crop or rotate a landscape screenshot.
- Keep a clear header, content region and bottom action/navigation region.
- Put the page title and global context in stable positions between sibling
  pages. Keep primary actions reachable and at least 48 px high.
- Lists show a bounded number of rows. Preserve row rhythm and make overflow a
  product decision rather than allowing content to collide.
- Charts must remain readable without color. Label time range and distinguish
  missing data from zero.
- Long bilingual copy must use the shared wrapping and truncation helpers in
  `src/ui/text_layout.*`.

## Interaction

- Visual controls and hit targets share one rectangle definition.
- A tap commits a discrete action after release. Mutations give immediate visual
  confirmation and use a partial refresh when safe.
- The top button toggles Launcher; double-click returns to Baby Week. Bottom
  edge swipe up opens Launcher and a qualifying downward swipe closes it.
- Orientation follows the settled IMU result. Re-render for the new logical
  size only after orientation is stable.
- Editors that could lose an in-progress value should prevent automatic sleep.
  Back must either preserve the prior value or make cancellation explicit.

## E-paper refresh policy

| Situation | Refresh |
| --- | --- |
| First stable page, orientation change, sleep frame | Full monochrome |
| Tap mutation, counter update, small form change | Partial monochrome |
| Repeated fast transitions or visible ghosting | Cleanup full refresh |
| Explicit Settings cleanup action | Clear/full refresh |

Never trigger a panel refresh from an ISR, input callback, page renderer or
background sensor task. Those layers post/update state; the owning app or
coordinator performs the refresh.

## Content and assets

- UI identifiers, source strings and logs are English; user-visible strings are
  exposed in English and Simplified Chinese through `src/ui/ui_language.*`.
- Generated CJK glyphs and 1-bit images are source-controlled for deterministic
  builds. Commit both editable source and generated output.
- Pregnancy week images are normalized to 148×148 1-bit assets. Preserve the
  distinct week mapping in `src/pregnancy/content/pregnancy_week_visual.*`.
- Never use color alone, tiny legends or medical-looking scoring to describe
  kicks, weight or pregnancy progress.

## Review gate

A UI change is complete only when landscape and portrait composition, English
and Chinese strings, tap hit maps, full/partial refresh behavior, sleep/wake
restoration and real-device ghosting have been considered. Use
`docs/EINK_UI_REAL_DEVICE_TEST.md` for physical evidence.

