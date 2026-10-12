from __future__ import annotations

import math
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools import measure_gpu_workload as workload


class WorkloadAccountingTests(unittest.TestCase):
    def test_energy_units_and_reset(self):
        measured = workload.energy_interval((1_000, 10_000_000), (1_000_001_000, 30_000_000))
        self.assertAlmostEqual(measured["joules"], 3.6)
        self.assertAlmostEqual(measured["watts"], 1.8)
        self.assertEqual(workload.energy_interval((0, 0), (0, 10_000_000))["watts"], 0)
        for before, after in [((10, 1), (9, 2)), ((10, 1), (11, 1)), ((10, 2), (11, 1))]:
            self.assertIsNone(workload.energy_interval(before, after)["watts"])

    def test_gpu_aggregation_preserves_physical_engines_and_adapters(self):
        def row(pid, adapter, engine, kind, value, status=0):
            return {"instance": f"pid_{pid}_luid_{adapter}_phys_0_eng_{engine}_engtype_{kind}",
                    "value": value, "status": status}

        rows = [row(1, "A", 0, "3D", 20), row(2, "A", 0, "3D", 30, 1),
                row(1, "A", 1, "Copy", 40), row(1, "B", 0, "3D", 60),
                row(3, "A", 0, "3D", 90), row(1, "A", 0, "3D", 90, 5),
                row(1, "A", 0, "3D", math.nan), row(1, "A", 0, "3D", -1)]
        measured = workload.engine_totals(rows, {1, 2})
        self.assertEqual(measured["engines_percent"], {"luid_A_phys_0:eng_0:3D": 50,
                                                     "luid_A_phys_0:eng_1:Copy": 40,
                                                     "luid_B_phys_0:eng_0:3D": 60})
        self.assertEqual(measured["busiest_engine_percent"], {"luid_A_phys_0": 50, "luid_B_phys_0": 60})
        self.assertEqual(measured["rejected_instances"], 3)

    def test_cpu_normalization_and_process_replacement(self):
        old = {"status": "ok", "creation_filetime": 100, "cpu_seconds": 4}
        current = old | {"cpu_seconds": 6}
        self.assertEqual(workload.process_interval(current, old, 2, 8, None, None)["cpu_machine_percent"], 12.5)
        for item in [current | {"creation_filetime": 200}, current | {"cpu_seconds": 3},
                     current | {"cpu_seconds": math.inf}, {"status": "exited"}]:
            self.assertIsNone(workload.process_interval(item, old, 2, 8, None, None)["cpu_machine_percent"])

    def test_pdh_fallback_requires_continuous_process_identity(self):
        denied = {"status": "open_failed", "error": 5, "cpu_seconds": None}
        previous = {"instance": "dwm:2", "elapsed_seconds": 10, "cpu_core_percent": 40,
                    "private_bytes": 100, "working_set_bytes": 200}
        current = previous | {"elapsed_seconds": 11}
        measured = workload.process_interval(denied, denied, 1, 8, current, previous)
        self.assertEqual(measured["cpu_machine_percent"], 5)
        self.assertEqual(measured["error"], 5)
        self.assertEqual(measured["cpu_source"], "pdh_process_v2")
        for counter in [None, current | {"elapsed_seconds": 1}, current | {"elapsed_seconds": None},
                        current | {"instance": "replacement:2"}, current | {"cpu_core_percent": math.nan},
                        current | {"cpu_core_percent": -1}]:
            self.assertIsNone(workload.process_interval(denied, denied, 1, 8, counter, previous)["cpu_machine_percent"])
        self.assertIsNone(workload.process_interval({"status": "exited"}, denied, 1, 8, current, previous)["cpu_machine_percent"])

    def test_counter_join_keeps_missing_values_and_limits_saved_processes(self):
        def row(instance, value, status=0):
            return {"instance": instance, "value": value, "status": status}

        raw = {"errors": {"gpu_shared_bytes": 42}, "counters": {
            "process_id": [row("wanted:2", 2), row("other:3", 3), row("invalid", math.inf)],
            "process_cpu": [row("other:3", 10), row("wanted:2", 20)],
            "process_elapsed": [row("wanted:2", 10)],
            "process_private_bytes": [row("wanted:2", 500, 5)],
            "gpu_engines": [row("pid_2_luid_A_phys_0_eng_0_engtype_3D", 10),
                            row("pid_3_luid_A_phys_0_eng_0_engtype_3D", 20)]}}
        processes = workload.process_counter_map(raw)
        self.assertEqual(processes[2]["cpu_core_percent"], 20)
        self.assertIsNone(processes[2]["private_bytes"])
        saved = workload.selected_counter_rows(raw, {2}, processes)
        self.assertEqual([item["instance"] for item in saved["counters"]["process_cpu"]], ["wanted:2"])
        self.assertEqual(len(saved["counters"]["gpu_engines"]), 1)
        self.assertEqual(saved["errors"], raw["errors"])


if __name__ == "__main__":
    unittest.main()
