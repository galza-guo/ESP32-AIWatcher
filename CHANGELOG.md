# Changelog

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
