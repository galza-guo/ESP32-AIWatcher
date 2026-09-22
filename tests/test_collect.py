#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "host" / "desk_monitor.py"
sys.path.insert(0, str(ROOT / "host"))


def load_mod():
    spec = importlib.util.spec_from_file_location("desk_monitor", MODULE)
    mod = importlib.util.module_from_spec(spec)
    sys.modules["desk_monitor"] = mod
    spec.loader.exec_module(mod)
    return mod


class CollectTests(unittest.TestCase):
    def test_cpu_times(self):
        mod = load_mod()
        times = mod.cpu_times()
        self.assertIsNotNone(times)
        idle, total = times
        self.assertGreater(total, 0)
        self.assertGreaterEqual(total, idle)

    def test_mem_percent(self):
        mod = load_mod()
        pct = mod.mem_percent()
        self.assertIsNotNone(pct)
        self.assertGreaterEqual(pct, 0)
        self.assertLessEqual(pct, 100)

    def test_hwmon_and_sample(self):
        mod = load_mod()
        collector = mod.Collector()
        collector.sample_sys()
        collector.sample_sys()
        snap = collector.snapshot()
        self.assertIn("sys", snap)
        self.assertIn("cpu", snap["sys"])
        self.assertTrue(snap["history"]["mem"])

    def test_ai_labels(self):
        mod = load_mod()
        payload = mod.load_ai()
        ids = [row["id"] for row in payload["providers"]]
        self.assertEqual(ids, ["codex", "grok", "minimax"])
        grok = next(row for row in payload["providers"] if row["id"] == "grok")
        self.assertEqual(grok["label"], "Grok")

    def test_network_serial_payload(self):
        collector = load_mod().Collector()
        collector.network = {"down": 1234.7, "up": None, "rx": 2**40, "tx": 2**39}
        message = json.loads(collector.serial_sys_line())
        self.assertEqual(message["nd"], 1235)
        self.assertIsNone(message["nu"])
        self.assertEqual(message["nr"], 2**40)
        self.assertEqual(message["nt"], 2**39)


if __name__ == "__main__":
    unittest.main()
