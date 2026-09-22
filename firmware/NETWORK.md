# Network card

System shows one Network card with separate Download and Upload columns.
Each column shows bytes per second, then today's transferred bytes. Units are
decimal B/KB/MB/GB (1000 bytes per KB), not bits per second. The four original
metrics remain visible above it, with a more compact version of the same design.

The Linux host reads receive/transmit counters from `/proc/net/dev`, summing
interfaces with a hardware `device` link in `/sys/class/net`. This includes
Ethernet, Wi-Fi and WWAN and excludes loopback, VPN tunnels, bridges and
container interfaces to avoid counting those software layers again. Counts
include local-network traffic and are not an ISP billing measurement.
See the [Linux statistics documentation](https://www.kernel.org/doc/html/latest/networking/statistics.html).

Rates use monotonic elapsed time. A first sample, missing source, or newly
appearing interface establishes a baseline without inventing a rate. Counter
resets never produce negative throughput or reduce accumulated totals.

Daily totals use the host's local calendar date. They start when this feature
is first enabled; earlier traffic is not reconstructed. Checkpoints store both
totals and interface baselines every 30 seconds using an atomic file replacement:

```
$XDG_STATE_HOME/esp32-aiwatcher/network.json
# Default: ~/.local/state/esp32-aiwatcher/network.json
```

A restart on the same boot/day catches up from the saved counters without
double counting; the first rate after a restart is unavailable. Across a host
reboot, saved daily totals are retained but old interface baselines are discarded.
At midnight totals reset and a fresh baseline prevents a cross-day gap from
being attributed to the new day. Traffic before initialization, during an
unobserved interface reset, or in unsaved intervals before a reboot may be missed.

Serial System messages add `nd`/`nu` (download/upload bytes per second), and
`nr`/`nt` (64-bit received/transmitted daily totals). The HTTP snapshot exposes
these as `network.down/up/rx/tx`. Older hosts show `--` on the Network card.

Validation covers rates, restart catch-up, reboot baselines, midnight reset,
hotplug, counter resets, missing/malformed counters, physical-interface filtering,
64-bit totals, serial serialization, and full/incremental pixel equivalence.
Both theme previews were visually checked. The firmware was flashed with hash
verification; device logs confirm the larger internal tile and successful touch
initialization. Live network rates were verified through the host API, and an
actual collector restart retained both daily counters and resumed rate reporting.
