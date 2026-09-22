"""Physical-interface throughput and locally persisted daily transfer totals."""
from __future__ import annotations

import datetime as dt
import json
import os
import sys
import time
from pathlib import Path


def read_counters(proc: Path = Path("/proc/net/dev"),
                  sysfs: Path = Path("/sys/class/net")) -> dict[str, tuple[int, int]] | None:
    try:
        lines = proc.read_text().splitlines()
    except OSError:
        return None
    result = {}
    for line in lines:
        if ":" not in line:
            continue
        name, data = line.rsplit(":", 1)
        name = name.strip()
        # Hardware devices only: excludes lo, VPN, bridges and container veths.
        if not (sysfs / name / "device").exists():
            continue
        fields = data.split()
        try:
            rx, tx = int(fields[0]), int(fields[8])
        except (ValueError, IndexError):
            continue
        if min(rx, tx) >= 0:
            result[name] = (rx, tx)
    return result


class NetworkMeter:
    def __init__(self, state_path: Path | None, boot_id: str):
        self.path = state_path
        self.boot_id = boot_id
        self.day = ""
        self.rx = self.tx = 0
        self.previous: dict[str, tuple[int, int]] = {}
        self.last_mono: float | None = None
        self.last_save = 0.0
        if self.path is None:
            return
        try:
            saved = json.loads(self.path.read_text())
            if saved.get("version") != 1:
                return
            day = saved["day"]
            dt.date.fromisoformat(day)
            rx, tx = saved["rx"], saved["tx"]
            counters = saved["counters"]
            if not isinstance(rx, int) or not isinstance(tx, int) or min(rx, tx) < 0:
                return
            previous = {}
            for name, pair in counters.items():
                if not isinstance(name, str) or len(pair) != 2:
                    return
                if not all(isinstance(n, int) and n >= 0 for n in pair):
                    return
                previous[name] = tuple(pair)
            self.day, self.rx, self.tx = day, rx, tx
            if boot_id != "unknown" and saved.get("boot_id") == boot_id:
                self.previous = previous
        except (OSError, ValueError, KeyError, TypeError, AttributeError):
            pass

    def sample(self, counters: dict[str, tuple[int, int]] | None,
               now: float | None = None, today: str | None = None) -> dict:
        now = time.monotonic() if now is None else now
        today = dt.date.today().isoformat() if today is None else today
        new_day = self.day != today
        if new_day:
            self.day, self.rx, self.tx = today, 0, 0
            # Never attribute a gap spanning midnight to the new day.
            self.previous = {}
            self.last_mono = None
        if counters is None:
            self.last_mono = None
            return {"down": None, "up": None, "rx": self.rx, "tx": self.tx}
        rx_delta = tx_delta = 0
        comparable = False
        for name, (rx, tx) in counters.items():
            old = self.previous.get(name)
            if old is not None:
                comparable = True
                # Reset/recreated interfaces establish a fresh baseline per direction.
                rx_delta += max(0, rx-old[0])
                tx_delta += max(0, tx-old[1])
        self.rx += rx_delta
        self.tx += tx_delta
        elapsed = None if self.last_mono is None else now-self.last_mono
        valid_rate = comparable and elapsed is not None and elapsed > 0
        result = {"down": rx_delta/elapsed if valid_rate else None,
                  "up": tx_delta/elapsed if valid_rate else None,
                  "rx": self.rx, "tx": self.tx}
        self.previous = dict(counters)
        self.last_mono = now
        if new_day or now-self.last_save >= 30:
            self.save(now)
        return result

    def save(self, now: float | None = None) -> bool:
        if not self.day or self.path is None:
            return True
        payload = {"version": 1, "day": self.day, "boot_id": self.boot_id,
                   "rx": self.rx, "tx": self.tx, "counters": self.previous}
        temporary = self.path.with_suffix(".tmp")
        try:
            self.path.parent.mkdir(parents=True, exist_ok=True)
            with temporary.open("w") as f:
                json.dump(payload, f, separators=(",", ":"))
                f.flush()
                os.fsync(f.fileno())
            temporary.replace(self.path)
            self.last_save = time.monotonic() if now is None else now
            return True
        except OSError as error:
            self.last_save = time.monotonic() if now is None else now
            print(f"network totals: save failed: {error}", file=sys.stderr)
            return False
