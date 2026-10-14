# E-ink refresh strategy

The existing Sticky Core display owner remains unchanged.

- Cold boot clears the physical panel once; deep-sleep wake preserves the image.
- Stable page entry and every page change use a full monochrome refresh.
- Small state changes use the SSD1677 partial waveform.
- The driver sends a complete 1-bpp comparison frame for partial refresh because
  the controller applies its partial waveform across the panel.
- Page changes are full refreshes in every app and in the launcher; only changes
  inside the page already on screen use partial refresh. `PageRefreshTracker`
  (`src/ui/refresh_policy.h`) makes that decision. The display layer still
  promotes the ninth consecutive partial refresh to a full one.
- `sticky_display_prepare_app_transition_refresh()` exists in the display layer
  but nothing calls it, so there is no fast app-transition refresh today.
- Settings exposes an explicit display cleanup action.
- Sleep draws a small indicator, performs a stable full refresh, sleeps the
  controller and removes panel power.

Pages render only on entry, lifecycle resume, user mutation or meaningful clock
rollover. Their polling loops do not continuously redraw.
