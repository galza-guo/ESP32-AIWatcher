# Changelog

## v1.1.0 — 2026-09-22

- Add a single-page Settings panel with persistent brightness, Light/Dark
  appearance, idle sleep, and read-only firmware/connection details.
- Improve dark-mode legibility with heavier text strokes and brighter labels.
- Add Network rates with drawn direction arrows and paired history traces;
  daily transfer totals persist across collector restarts.
- Keep the existing double-framebuffer RGB presentation and touch handling.

Validation: ten host tests, firmware build, flash hash verification, native
previews in both themes, and full/incremental/strip pixel checks pass. Settings
were previously confirmed by the user; timed sleep/wake and the final dark
visual adjustment have not yet received separate physical confirmation.

Upgrade the host collector together with the firmware for Network readings.

## v1.0.0 — 2026-09-21

First stable release for the EYA EA4313-S3 800×480 touch display.

- Fix periodic System-page tearing and tearing during tab switches with two
  framebuffers, frame-boundary presentation and internal RGB DMA bounce buffers.
- Restore GT911 touch with the board's verified initialization sequence.
- Add a minimal light System / Usage interface and shared native preview.
- Collect Linux system metrics and local GPT/Codex, Grok and MiniMax usage.
- Make `s3-eya-rgb-bounce` the default build; retain the legacy environment.

Hardware confirmation: the user reported that System refresh and tab-switch
tearing were completely resolved. Firmware build, flash verification, four
host tests and native rendering / buffer synchronization assertions passed.
