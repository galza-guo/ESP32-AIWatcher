# Desk Monitor

A small Omarchy instrument for this ThinkPad: CPU, memory, temperature and fan
as live traces, plus GPT / Grok / MiniMax usage from the existing agent table.

```
http://127.0.0.1:7824/
```

The collector reads `/proc`, hwmon, and `~/.local/state/omarchy/agents/usage`.
If the ESP32-S3 is on USB it also writes the same JSON to `/dev/ttyUSB0`.

```bash
systemctl --user enable --now desk-monitor.service
desk-monitor-ui
```

Toggle the window with Super+Alt+D.
