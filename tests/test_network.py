import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "host"))
from network import NetworkMeter, read_counters


class NetworkTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "network.json"

    def test_rates_restart_and_midnight(self):
        meter = NetworkMeter(self.path, "boot-a")
        first = meter.sample({"eth0": (100, 200)}, 10, "2026-09-22")
        self.assertIsNone(first["down"])
        point = meter.sample({"eth0": (1100, 700)}, 12, "2026-09-22")
        self.assertEqual(point, {"down": 500, "up": 250, "rx": 1000, "tx": 500})
        meter.save(12)
        resumed = NetworkMeter(self.path, "boot-a")
        point = resumed.sample({"eth0": (1600, 800)}, 20, "2026-09-22")
        self.assertEqual((point["rx"], point["tx"]), (1500, 600))
        self.assertIsNone(point["down"]) # No invented rate across restart.
        point = resumed.sample({"eth0": (2000, 900)}, 22, "2026-09-23")
        self.assertEqual((point["rx"], point["tx"]), (0, 0))
        self.assertIsNone(point["down"])

    def test_reboot_reset_and_hotplug(self):
        meter = NetworkMeter(self.path, "boot-a")
        meter.sample({"eth0": (100, 100)}, 10, "2026-09-22")
        meter.sample({"eth0": (500, 600)}, 12, "2026-09-22")
        meter.save(12)
        reboot = NetworkMeter(self.path, "boot-b")
        point = reboot.sample({"eth0": (900, 900)}, 1, "2026-09-22")
        self.assertEqual((point["rx"], point["tx"]), (400, 500))
        point = reboot.sample({"eth0": (5, 8), "wlan0": (999999, 999999)}, 2, "2026-09-22")
        self.assertEqual(point["down"], 0)
        self.assertEqual(point["rx"], 400)
        point = reboot.sample({"eth0": (25, 18), "wlan0": (1000019, 1000039)}, 3, "2026-09-22")
        self.assertEqual((point["down"], point["up"]), (40, 50))
        point = reboot.sample({}, 4, "2026-09-22")
        self.assertIsNone(point["down"])
        point = reboot.sample({"eth0": (1000, 1000)}, 5, "2026-09-22")
        self.assertEqual(point["rx"], 440)

    def test_missing_sample_does_not_create_rate_spike(self):
        meter = NetworkMeter(self.path, "boot")
        meter.sample({"eth0": (100, 200)}, 1, "2026-09-22")
        self.assertIsNone(meter.sample(None, 2, "2026-09-22")["down"])
        point = meter.sample({"eth0": (1000, 2000)}, 3, "2026-09-22")
        self.assertIsNone(point["down"])
        self.assertEqual((point["rx"], point["tx"]), (900, 1800))

    def test_invalid_state_and_64_bit_totals(self):
        self.path.write_text('{"version":1,"rx":-4}')
        meter = NetworkMeter(self.path, "boot")
        meter.sample({"eth0": (0, 0)}, 1, "2026-09-22")
        meter.sample({"eth0": (2**40, 2**39)}, 2, "2026-09-22")
        meter.save(2)
        self.assertEqual(NetworkMeter(self.path, "boot").rx, 2**40)
        self.assertEqual(json.loads(self.path.read_text())["tx"], 2**39)

    def test_physical_only_and_malformed_lines(self):
        root = Path(self.temp.name)
        (root / "eth0/device").mkdir(parents=True)
        proc = root / "dev"
        proc.write_text("header\nlo: 999 0 0 0 0 0 0 0 999\n"
                        "tun0: 888 0 0 0 0 0 0 0 888\n"
                        "eth0: 123 0 0 0 0 0 0 0 456\n")
        self.assertEqual(read_counters(proc, root), {"eth0": (123, 456)})
        proc.write_text("eth0: broken\n")
        self.assertEqual(read_counters(proc, root), {})
        self.assertIsNone(read_counters(root/"missing", root))
