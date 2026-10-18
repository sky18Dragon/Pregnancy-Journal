# E-ink refresh strategy

The existing Sticky Core display owner remains unchanged.

- Cold boot clears the physical panel once; deep-sleep wake preserves the image.
- Stable page entry and every page change use a full monochrome refresh.
- Small state changes use the SSD1677 partial waveform.
- The driver sends a complete 1-bpp comparison frame for partial refresh because
  the controller applies its partial waveform across the panel.
- Page changes are full refreshes in every app; only changes inside the page
  already on screen use partial refresh. `PageRefreshTracker`
  (`src/ui/refresh_policy.h`) makes that decision. Exceptions are listed below.
- The display layer promotes a partial request to a full refresh after 10
  consecutive partial refreshes (`kPartialRefreshesBeforeCleanup`; it was 8
  before 2026-10-18). Pages keep no second count. Any full refresh, gray refresh
  or clear restarts both this count and the fast-transition count, and
  `sticky_display_partial_refreshes_since_full()` lets a caller see that its
  partial request was promoted.
- Fast app transitions: opening the launcher, returning to the app and switching
  apps arm `sticky_display_prepare_app_transition_refresh()` in the coordinator
  (`src/app/sticky_app.cpp`). The next monochrome refresh, normally the first
  frame, is then a partial one (about 0.9 s, no black flash); every sixth such
  transition is a full refresh. An arm that nobody takes within 4 s is dropped.
  Rotation changes, the sleep frame and page changes inside an app are not
  affected.
- Baby Week home <-> Baby page (trial): the change is a partial refresh with no
  pressed-state refresh, and after 5 s without activity one full refresh repaints
  the page (cancelled if the display layer already promoted a refresh to full).
  The pair is decided by `pregnancy_page_change_uses_partial()`.
- Loading pages: Weight and Kicks draw nothing while a cold card read is in
  flight, so the completion draws the content page (one refresh). A loading page
  appears only if the read takes longer than 1.5 s (`loading_page_due()` in
  `refresh_policy.h`) or when the user taps RETRY. Baby Week keeps the profile it
  already read, draws the home page at once and re-checks the card in the
  background; its loading page is used only for a cold read.
- Settings exposes an explicit display cleanup action.
- Sleep draws a small indicator, performs a stable full refresh, sleeps the
  controller and removes panel power.

Pages render only on entry, lifecycle resume, user mutation or meaningful clock
rollover. Their polling loops do not continuously redraw.
