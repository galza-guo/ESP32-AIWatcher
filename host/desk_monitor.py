#!/usr/bin/env python3
"""Omarchy desk monitor: system traces + agent usage over HTTP and USB serial."""

from __future__ import annotations

import json
import os
import sys
import termios
import threading
import time
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parents[1]
UI_DIR = ROOT / "ui"
USAGE_DIR = Path.home() / ".local" / "state" / "omarchy" / "agents" / "usage"
GROK_SESSIONS = Path.home() / ".grok" / "sessions"
HOST = "127.0.0.1"
PORT = 7824
HISTORY = 180
SYS_HZ = 2.0
AI_PERIOD = 15.0
SERIAL_CANDIDATES = (
    Path("/dev/serial/by-id/usb-1a86_USB_Serial-if00-port0"),
    Path("/dev/ttyUSB0"),
    Path("/dev/ttyACM0"),
)
WANTED_AI = ("codex", "grok", "minimax")
AI_LABELS = {"codex": "GPT", "grok": "Grok", "minimax": "MiniMax"}
TICKS_PER_USD = 10_000_000_000


def _read_text(path: Path) -> str | None:
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None


def _read_int(path: Path) -> int | None:
    raw = _read_text(path)
    if raw is None:
        return None
    try:
        return int(raw.strip().split()[0])
    except (TypeError, ValueError):
        return None


def cpu_times() -> tuple[int, int] | None:
    raw = _read_text(Path("/proc/stat"))
    if not raw:
        return None
    parts = raw.splitlines()[0].split()
    if parts[0] != "cpu" or len(parts) < 5:
        return None
    nums = [int(x) for x in parts[1:]]
    idle = nums[3] + (nums[4] if len(nums) > 4 else 0)
    return idle, sum(nums)


def mem_percent() -> float | None:
    raw = _read_text(Path("/proc/meminfo"))
    if not raw:
        return None
    total = available = None
    for line in raw.splitlines():
        if line.startswith("MemTotal:"):
            total = int(line.split()[1])
        elif line.startswith("MemAvailable:"):
            available = int(line.split()[1])
        if total is not None and available is not None:
            break
    if not total:
        return None
    return max(0.0, min(100.0, 100.0 * (1.0 - (available or 0) / total)))


def hwmon_map() -> dict[str, Path]:
    root = Path("/sys/class/hwmon")
    found: dict[str, Path] = {}
    if not root.is_dir():
        return found
    for entry in sorted(root.glob("hwmon*")):
        name = (_read_text(entry / "name") or "").strip()
        if name:
            found[name] = entry
    return found


def cpu_temp_c(hwmons: dict[str, Path]) -> float | None:
    core = hwmons.get("coretemp")
    if core is not None:
        for label in core.glob("temp*_label"):
            if (_read_text(label) or "").strip() == "Package id 0":
                milli = _read_int(core / (label.name.replace("_label", "_input")))
                if milli is not None:
                    return milli / 1000.0
        milli = _read_int(core / "temp1_input")
        if milli is not None:
            return milli / 1000.0
    thinkpad = hwmons.get("thinkpad")
    if thinkpad is not None:
        milli = _read_int(thinkpad / "temp1_input")
        if milli is not None:
            return milli / 1000.0
    return None


def fan_rpm(hwmons: dict[str, Path]) -> float | None:
    thinkpad = hwmons.get("thinkpad")
    if thinkpad is None:
        return None
    rpm = _read_int(thinkpad / "fan1_input")
    return None if rpm is None else float(rpm)


def grok_today_usd() -> float | None:
    if not GROK_SESSIONS.is_dir():
        return None
    today = time.strftime("%Y-%m-%d")
    total = 0
    found = False
    for path in GROK_SESSIONS.rglob("usage.json"):
        raw = _read_text(path)
        if not raw:
            continue
        try:
            data = json.loads(raw)
        except json.JSONDecodeError:
            continue
        if not isinstance(data, dict):
            continue
        session = data.get("session") or {}
        ticks = session.get("costUsdTicks")
        if not isinstance(ticks, (int, float)):
            continue
        updated = str(data.get("updatedAt") or "")[:10]
        if updated != today:
            continue
        total += int(ticks)
        found = True
    if not found:
        return 0.0
    return total / TICKS_PER_USD


def load_ai() -> dict[str, Any]:
    providers = []
    for agent_id in WANTED_AI:
        path = USAGE_DIR / f"{agent_id}.json"
        data = {}
        raw = _read_text(path)
        if raw:
            try:
                parsed = json.loads(raw)
                if isinstance(parsed, dict):
                    data = parsed
            except json.JSONDecodeError:
                data = {}
        limits = []
        for row in data.get("limits") or []:
            if not isinstance(row, dict):
                continue
            limits.append(
                {
                    "label": row.get("title") or row.get("label") or "Limit",
                    "percent": float(row.get("percent") or 0),
                    "resetsAt": row.get("resetsAt"),
                }
            )
        recent = []
        for row in data.get("recentDays") or []:
            if not isinstance(row, dict):
                continue
            recent.append(
                {
                    "date": row.get("date"),
                    "value": float(row.get("messageCount") or 0),
                }
            )
        extra: dict[str, Any] = {}
        if agent_id == "grok":
            usd = grok_today_usd()
            if usd is not None:
                extra["todayUsd"] = round(usd, 2)
        providers.append(
            {
                "id": agent_id,
                "label": AI_LABELS[agent_id],
                "name": data.get("name") or AI_LABELS[agent_id],
                "ready": bool(data.get("ready")),
                "tier": data.get("tierLabel") or "",
                "todayTokens": int(data.get("todayTotalTokens") or 0),
                "todayPrompts": int(data.get("todayPrompts") or 0),
                "todaySessions": int(data.get("todaySessions") or 0),
                "limits": limits,
                "recent": recent,
                "updatedAt": data.get("updatedAt"),
                "status": data.get("usageStatusText") or "",
                **extra,
            }
        )
    return {"providers": providers, "source": str(USAGE_DIR)}


class Collector:
    def __init__(self) -> None:
        self.lock = threading.Lock()
        self.history = {
            "cpu": deque(maxlen=HISTORY),
            "mem": deque(maxlen=HISTORY),
            "temp": deque(maxlen=HISTORY),
            "fan": deque(maxlen=HISTORY),
        }
        self.latest: dict[str, Any] = {
            "cpu": None,
            "mem": None,
            "temp": None,
            "fan": None,
        }
        self.ai = load_ai()
        self.prev_cpu: tuple[int, int] | None = None
        self.serial_path: str | None = None
        self._stop = threading.Event()

    def sample_sys(self) -> None:
        times = cpu_times()
        cpu = None
        if times and self.prev_cpu:
            idle_d = times[0] - self.prev_cpu[0]
            total_d = times[1] - self.prev_cpu[1]
            if total_d > 0:
                cpu = max(0.0, min(100.0, 100.0 * (1.0 - idle_d / total_d)))
        if times:
            self.prev_cpu = times
        hwmons = hwmon_map()
        point = {
            "cpu": cpu,
            "mem": mem_percent(),
            "temp": cpu_temp_c(hwmons),
            "fan": fan_rpm(hwmons),
        }
        with self.lock:
            self.latest = point
            for key, value in point.items():
                if value is None:
                    continue
                self.history[key].append(round(float(value), 2))

    def sample_ai(self) -> None:
        payload = load_ai()
        with self.lock:
            self.ai = payload

    def snapshot(self, include_history: bool = True) -> dict[str, Any]:
        with self.lock:
            hist = (
                {key: list(values) for key, values in self.history.items()}
                if include_history
                else {}
            )
            return {
                "t": int(time.time() * 1000),
                "sys": dict(self.latest),
                "history": hist,
                "ai": dict(self.ai),
            }

    def serial_line(self) -> bytes:
        snap = self.snapshot(include_history=False)
        return (json.dumps({"v": 1, **snap}, separators=(",", ":")) + "\n").encode()


COLLECTOR = Collector()


def configure_serial(fd: int) -> None:
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[3] = 0
    attrs[4] = termios.B115200
    attrs[5] = termios.B115200
    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attrs)


def open_serial() -> tuple[int | None, str | None]:
    for path in SERIAL_CANDIDATES:
        if not path.exists():
            continue
        try:
            fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            configure_serial(fd)
            return fd, str(path)
        except OSError:
            continue
    return None, None


def collector_loop() -> None:
    last_ai = 0.0
    serial_fd: int | None = None
    serial_path: str | None = None
    last_serial_try = 0.0
    COLLECTOR.sample_sys()
    COLLECTOR.sample_ai()
    while not COLLECTOR._stop.is_set():
        COLLECTOR.sample_sys()
        now = time.time()
        if now - last_ai >= AI_PERIOD:
            COLLECTOR.sample_ai()
            last_ai = now
        if serial_fd is None and now - last_serial_try >= 2.0:
            serial_fd, serial_path = open_serial()
            COLLECTOR.serial_path = serial_path
            last_serial_try = now
        if serial_fd is not None:
            try:
                os.write(serial_fd, COLLECTOR.serial_line())
            except OSError:
                os.close(serial_fd)
                serial_fd = None
                serial_path = None
                COLLECTOR.serial_path = None
        COLLECTOR._stop.wait(1.0 / SYS_HZ)
    if serial_fd is not None:
        os.close(serial_fd)


class Handler(BaseHTTPRequestHandler):
    server_version = "DeskMonitor/1.0"

    def log_message(self, fmt: str, *args: Any) -> None:
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))

    def _send(self, status: int, body: bytes, content_type: str) -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        path = parsed.path
        if path == "/api/state":
            body = json.dumps(COLLECTOR.snapshot()).encode()
            self._send(200, body, "application/json; charset=utf-8")
            return
        if path == "/api/health":
            payload = {
                "ok": True,
                "serial": COLLECTOR.serial_path,
                "port": PORT,
            }
            self._send(200, json.dumps(payload).encode(), "application/json")
            return
        if path == "/events":
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-cache")
            self.send_header("Connection", "keep-alive")
            self.end_headers()
            try:
                while not COLLECTOR._stop.is_set():
                    body = json.dumps(COLLECTOR.snapshot())
                    self.wfile.write(f"data: {body}\n\n".encode())
                    self.wfile.flush()
                    time.sleep(1.0 / SYS_HZ)
            except (BrokenPipeError, ConnectionResetError, OSError):
                return
            return
        relative = "index.html" if path in ("/", "") else path.lstrip("/")
        target = (UI_DIR / relative).resolve()
        if UI_DIR not in target.parents and target != UI_DIR:
            self._send(403, b"forbidden", "text/plain")
            return
        if not target.is_file():
            self._send(404, b"not found", "text/plain")
            return
        suffix = target.suffix.lower()
        types = {
            ".html": "text/html; charset=utf-8",
            ".css": "text/css; charset=utf-8",
            ".js": "text/javascript; charset=utf-8",
            ".svg": "image/svg+xml",
        }
        self._send(200, target.read_bytes(), types.get(suffix, "application/octet-stream"))


def main() -> int:
    if not UI_DIR.is_dir():
        print(f"missing UI directory: {UI_DIR}", file=sys.stderr)
        return 1
    worker = threading.Thread(target=collector_loop, name="collector", daemon=True)
    worker.start()
    server = ThreadingHTTPServer((HOST, PORT), Handler)
    server.daemon_threads = True
    print(f"desk-monitor http://{HOST}:{PORT}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        COLLECTOR._stop.set()
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
