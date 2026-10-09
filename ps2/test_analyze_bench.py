"""Regression tests for machine-readable PS2 EE benchmark logs."""
import unittest

from analyze_bench import parse, analyze


class BenchLogTests(unittest.TestCase):
    def setUp(self):
        self.valid = [
            "PASS: 311600 PS2 EE MMI filter cases",
            "EXTRA_PASS,write=131200,palette=32800",
            "FULL_PASS,write=100,palette=200,convert=300,adam7=400",
            "COLOR_PASS,cases=500",
            "BENCH_INFO,unit=clock_ticks,clock_per_sec=1000000,repeats=3",
            "BENCH_HEADER,variant,filter,bpp,rowbytes,row_align,prev_align,"
            "loops,copy_ticks,scalar_ticks,optimized_ticks,"
            "scalar_net_ticks,optimized_net_ticks,optimized_ticks_per_byte_x1000",
            "BENCH,sub4,0,4,64,0,0,512,20,120,70,100,50,1525",
            "BENCH,sub4-prefix-direct,0,4,64,0,0,512,20,120,60,100,40,1220",
            "BENCH_DONE,passed=2,failed=0,sink=99",
        ]

    def test_parse_and_speedup(self):
        rows, issues, statuses, timer, length = parse(self.valid)
        self.assertEqual(len(rows), 2)
        self.assertFalse(issues)
        self.assertEqual(timer, "clock_ticks")
        self.assertEqual(statuses["correctness_pass_lines"], 1)
        self.assertEqual(statuses["extra_pass_lines"], 1)
        self.assertEqual(statuses["full_pass_lines"], 1)
        self.assertEqual(statuses["color_pass_lines"], 1)
        self.assertEqual(statuses["benchmark_runs_completed"], 1)
        self.assertEqual(length, 9)
        self.assertEqual(rows[0].speedup, 2.0)
        self.assertEqual(rows[1].speedup, 2.5)
        self.assertEqual(analyze(self.valid, top=0)[0], 0)

    def test_explicit_failure_is_reported(self):
        lines = self.valid + ["BENCH_FAIL,sub8,8,1024,1"]
        self.assertEqual(analyze(lines, top=0)[0], 1)

    def test_missing_done_is_incomplete(self):
        self.assertEqual(analyze(self.valid[:-1], top=0)[0], 1)

    def test_malformed_or_zero_duration(self):
        lines = self.valid[:3] + [
            "BENCH,broken,not-an-int",
            "BENCH,slow,0,4,64,0,0,512,20,20,20,0,0,0",
        ] + self.valid[-1:]
        rows, issues, _, _, _ = parse(lines)
        self.assertEqual(len(rows), 1)
        self.assertEqual(len(issues), 1)
        self.assertIsNone(rows[0].speedup)


if __name__ == "__main__":
    unittest.main()
