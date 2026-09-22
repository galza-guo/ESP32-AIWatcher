# Display settings — 1.1.0 development build

Open Settings using the gear at the bottom left. The top card is read-only:
firmware version, host connection state and age of the latest system sample.
There are no nested menus. Host connection is inferred from incoming System
messages: waiting before the first sample, connected while samples are less
than three seconds old, disconnected afterward. This indicates data freshness,
not electrical USB detection. Status refreshes once a second on this page.

Controls:

- Brightness: 10–100%, using 12 kHz / 8-bit LEDC PWM on GPIO 1.
- Appearance: Light or Dark, applied across System, Usage and Settings.
- Sleep after: Never (default), 5 minutes, or 15 minutes of no touch/button
  activity. Only the backlight turns off; serial collection and touch remain
  active. The entire first touch gesture wakes the display without activating
  a control. BOOT can also wake it without switching tabs.

Preferences are stored as one versioned, validated 32-bit value in the device's
NVS (`aiwatcher/display`). Writes are coalesced until at least 1.2 seconds after
the last change and touch release. Identical values are not rewritten. Invalid
stored values fall back to 100% brightness, Light and Never. Leave a moment
after editing before removing power. Stored preferences survive ordinary
app-only firmware uploads; erasing/replacing NVS resets them.

The existing double-framebuffer ownership and presentation logic is unchanged.
The stable v1.0.0 tag and release remain available for rollback.

Validation so far: firmware build and app flash verification; PWM, NVS and
touch startup logs; native previews of both themes; full/strip pixel equivalence
for all pages and connection states; slider hit/drag capture; preference
roundtrips and invalid values; idle deadlines including millis wraparound;
wake gesture suppression and button wake. Four Python host tests pass.
The user confirmed the controls look good; device logs show successful setting
writes and restoration of 22% brightness after an app update and collector
restart. Timed sleep/wake still requires a separate physical confirmation.
