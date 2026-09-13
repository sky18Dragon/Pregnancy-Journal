# E-ink refresh strategy

The existing Sticky Core display owner remains unchanged.

- Cold boot clears the physical panel once; deep-sleep wake preserves the image.
- Stable page entry uses a full monochrome refresh.
- Small state changes use the SSD1677 partial waveform.
- The driver sends a complete 1-bpp comparison frame for partial refresh because
  the controller applies its partial waveform across the panel.
- App transitions use fast refresh, with a cleanup full refresh after five fast
  transitions to control ghosting.
- Settings exposes an explicit display cleanup action.
- Sleep draws a small indicator, performs a stable full refresh, sleeps the
  controller and removes panel power.

Pages render only on entry, lifecycle resume, user mutation or meaningful clock
rollover. Their polling loops do not continuously redraw.
