# Desktop Pet Source Library

This library keeps source material separate from the packed firmware assets.

- `dialogue/openclaw_rabbit.yaml`, `openclaw_claw.yaml`, and
  `openclaw_dachshund.yaml` preserve the three audited upstream phrase books
  with provenance.
- `rules/tamapoke_behavior.json` records the imported behavior constants in a
  machine-readable form.
- Firmware-ready content lives in `src/apps/desktop_pet/core/` as compact
  fixed-size C++ tables.
- Visual source art remains under `assets/desktop_pet/source/` and generated
  one-bit assets remain under `assets/desktop_pet/firmware/`.

The build includes only the compact C++ tables. The source archive therefore
does not consume device Flash or RAM.

The pet's food value is driven by elapsed time and care actions. Feeding raises
the value directly, so the complete feeding loop can be tested independently
from the device battery. Calendar progression receives trusted time from the
Sticky PCF8563 RTC adapter.
