# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Host unit tests for muse_pet.c — decay, clamping, actions, offline decay, write throttle."""

import json
import os
import subprocess
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HARNESS_C = os.path.join(ROOT, "tests", "muse_pet_harness.c")
COMPONENT_DIR = os.path.join(ROOT, "components", "muse")
HOST_COMPAT = os.path.join(ROOT, "tests", "host_compat.h")
PET_FAKES = os.path.join(ROOT, "tests", "pet_fakes")


def _build_harness():
    exe = tempfile.NamedTemporaryFile(delete=False, suffix="")
    exe.close()
    subprocess.check_call(
        [
            "cc",
            "-include", HOST_COMPAT,
            "-std=gnu11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I", PET_FAKES,
            "-I", COMPONENT_DIR,
            HARNESS_C,
            "-o", exe.name,
            "-lm",
        ]
    )
    return exe.name


_EXE = None


def get_exe():
    global _EXE
    if _EXE is None:
        _EXE = _build_harness()
    return _EXE


class Harness:
    def __init__(self):
        self._proc = subprocess.Popen(
            [get_exe()],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
        )

    def cmd(self, *lines):
        for line in lines:
            self._proc.stdin.write(line + "\n")
        self._proc.stdin.flush()

    def stats(self):
        self.cmd("s")
        return json.loads(self._proc.stdout.readline())

    def nwrites(self):
        self.cmd("nwrites")
        return int(self._proc.stdout.readline().strip())

    def reset_writes(self):
        self.cmd("reset_writes")

    def close(self):
        self._proc.stdin.close()
        self._proc.wait()


class TestMusePetDecay(unittest.TestCase):
    def setUp(self):
        self.h = Harness()

    def tearDown(self):
        self.h.close()

    def test_initial_stats_full(self):
        """Fresh init returns all stats = 1.0 (full)."""
        self.h.cmd("t 0", "w 0", "init")
        st = self.h.stats()
        self.assertAlmostEqual(st["hunger"], 1.0, places=3)
        self.assertAlmostEqual(st["energy"], 1.0, places=3)
        self.assertAlmostEqual(st["mood"], 1.0, places=3)

    def test_decay_after_one_hour(self):
        """Stats should decrease after one hour of ticks."""
        self.h.cmd("t 0", "w 0", "init")
        # advance 1 hour in microseconds
        self.h.cmd(f"t {3600 * 1_000_000}", "tick")
        st = self.h.stats()
        # hunger decays ~2 pts/hr on 0-100 scale = 0.02 on 0-1 scale
        self.assertLess(st["hunger"], 1.0)
        self.assertGreater(st["hunger"], 0.9)
        self.assertLess(st["energy"], 1.0)
        self.assertLess(st["mood"], 1.0)

    def test_no_negative_values(self):
        """Stats clamp to 0, never go negative."""
        self.h.cmd("t 0", "w 0", "init")
        # advance 500 hours
        self.h.cmd(f"t {500 * 3600 * 1_000_000}", "tick")
        st = self.h.stats()
        self.assertGreaterEqual(st["hunger"], 0.0)
        self.assertGreaterEqual(st["energy"], 0.0)
        self.assertGreaterEqual(st["mood"], 0.0)

    def test_no_values_above_one(self):
        """Stats clamp to 1.0, never exceed full."""
        self.h.cmd("t 0", "w 0", "init")
        # No time has passed — everything should be 1.0
        self.h.cmd("tick")
        st = self.h.stats()
        self.assertLessEqual(st["hunger"], 1.0)
        self.assertLessEqual(st["energy"], 1.0)
        self.assertLessEqual(st["mood"], 1.0)


class TestMusePetActions(unittest.TestCase):
    def setUp(self):
        self.h = Harness()

    def tearDown(self):
        self.h.close()

    def test_feed_raises_hunger(self):
        """muse_pet_feed() should raise hunger."""
        self.h.cmd("t 0", "w 0", "init")
        # decay a bit first so there is room to rise
        self.h.cmd(f"t {10 * 3600 * 1_000_000}", "tick")
        before = self.h.stats()["hunger"]
        self.h.cmd("feed")
        after = self.h.stats()["hunger"]
        self.assertGreater(after, before)

    def test_pet_raises_mood(self):
        """muse_pet_pet() should raise mood."""
        self.h.cmd("t 0", "w 0", "init")
        self.h.cmd(f"t {10 * 3600 * 1_000_000}", "tick")
        before = self.h.stats()["mood"]
        self.h.cmd("pet")
        after = self.h.stats()["mood"]
        self.assertGreater(after, before)

    def test_feed_clamps_at_one(self):
        """Feeding when already full should not push hunger above 1.0."""
        self.h.cmd("t 0", "w 0", "init")
        for _ in range(10):
            self.h.cmd("feed")
        st = self.h.stats()
        self.assertLessEqual(st["hunger"], 1.0)

    def test_sleep_restores_energy(self):
        """Energy should restore while asleep and decay while awake."""
        self.h.cmd("t 0", "w 0", "init")
        # Drain energy for 24 hours awake
        self.h.cmd(f"t {24 * 3600 * 1_000_000}", "tick")
        drained = self.h.stats()["energy"]
        self.assertLess(drained, 1.0)

        # Switch to sleep and advance 2 hours
        self.h.cmd("sleep 1")
        self.h.cmd(f"t {(24 + 2) * 3600 * 1_000_000}", "tick")
        restored = self.h.stats()["energy"]
        self.assertGreater(restored, drained)


class TestMusePetOfflineDecay(unittest.TestCase):
    def setUp(self):
        self.h = Harness()

    def tearDown(self):
        self.h.close()

    def test_offline_decay_applied_on_init(self):
        """On re-init with a valid saved timestamp and a valid wall clock that is
        ahead of it, stats should have decayed by the offline interval."""
        MIN_VALID = 1577836800  # 2020-01-01
        saved_ts = MIN_VALID + 3600  # 1 hour after epoch floor
        current_ts = saved_ts + 2 * 3600  # 2 hours later

        # First boot: init, let stats decay a bit, then save (feed triggers commit)
        self.h.cmd(f"t 0", f"w {saved_ts}", "init")
        # Force a commit by feeding (immediate write)
        self.h.cmd("feed")
        # Record hunger after first init+feed
        st1 = self.h.stats()

        # Second boot at current_ts — stats should have decayed by 2 hours offline
        self.h.cmd(f"t {10 * 1_000_000}", f"w {current_ts}", "init")
        st2 = self.h.stats()
        # hunger was boosted by feed, now decayed for 2h offline — should be lower
        self.assertLess(st2["hunger"], st1["hunger"])
        self.assertLess(st2["mood"], st1["mood"])

    def test_no_offline_decay_without_valid_clock(self):
        """When wall clock is 0 (not synced), offline decay must not be applied."""
        # First boot with a valid ts, save via feed
        MIN_VALID = 1577836800
        self.h.cmd(f"t 0", f"w {MIN_VALID + 100}", "init", "feed")
        st1 = self.h.stats()

        # Second boot with wall clock = 0 (SNTP not synced)
        self.h.cmd(f"t {1_000_000}", "w 0", "init")
        st2 = self.h.stats()
        # Without valid wall clock, hunger should resume from what was saved (not decayed more)
        self.assertAlmostEqual(st2["hunger"], st1["hunger"], delta=0.05)


class TestMusePetWriteThrottle(unittest.TestCase):
    def setUp(self):
        self.h = Harness()

    def tearDown(self):
        self.h.close()

    def test_tick_does_not_write_immediately(self):
        """Calling tick once should not trigger an NVS write."""
        self.h.cmd("t 0", "w 0", "init")
        self.h.reset_writes()
        self.h.cmd("tick")
        n = self.h.nwrites()
        self.assertEqual(n, 0)

    def test_tick_writes_after_interval(self):
        """After WRITE_INTERVAL_US (3 minutes), tick should commit."""
        self.h.cmd("t 0", "w 0", "init")
        self.h.reset_writes()
        # Advance just over 3 minutes
        self.h.cmd(f"t {3 * 60 * 1_000_000 + 1}", "tick")
        n = self.h.nwrites()
        self.assertGreaterEqual(n, 1)

    def test_feed_writes_immediately(self):
        """feed() should commit to NVS immediately."""
        self.h.cmd("t 0", "w 0", "init")
        self.h.reset_writes()
        self.h.cmd("feed")
        n = self.h.nwrites()
        self.assertGreaterEqual(n, 1)

    def test_pet_writes_immediately(self):
        """pet() should commit to NVS immediately."""
        self.h.cmd("t 0", "w 0", "init")
        self.h.reset_writes()
        self.h.cmd("pet")
        n = self.h.nwrites()
        self.assertGreaterEqual(n, 1)


if __name__ == "__main__":
    unittest.main()
