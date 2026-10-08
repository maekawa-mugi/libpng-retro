#!/usr/bin/env python3
"""Summarize PS2 EE MMI test/benchmark serial logs.

Reads the unmodified stdout from one or more test_filter_mmi.elf runs.
No third-party Python modules or spreadsheet software are required.

    python3 ps2/analyze_bench.py ps2-output.txt
    python3 ps2/analyze_bench.py ps2-output.txt --csv ps2-results.csv
"""
from __future__ import annotations

import argparse
import csv
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Sample:
    variant: str
    filter_id: int
    bpp: int
    width: int
    row_align: int
    prev_align: int
    loops: int
    copy_ticks: int
    scalar_ticks: int
    optimized_ticks: int
    scalar_net: int
    optimized_net: int
    optimized_per_byte_x1000: int

    @property
    def speedup(self) -> float | None:
        if self.scalar_net <= 0 or self.optimized_net <= 0:
            return None
        return self.scalar_net / self.optimized_net


def parse(lines: list[str]) -> tuple[list[Sample], list[str], dict[str, int], str, int]:
    samples: list[Sample] = []
    issues: list[str] = []
    statuses: dict[str, int] = defaultdict(int)
    timer = "unknown"
    for line_no, line in enumerate(lines, 1):
        line = line.strip()
        if not line:
            continue
        if line.startswith("PASS: ") or line.startswith("PASS "):
            statuses["correctness_pass_lines"] += 1
        elif line.startswith("EXTRA_PASS,"):
            statuses["extra_pass_lines"] += 1
        elif line.startswith("FAIL") or line.startswith("BENCH_FAIL,") or line.startswith("EXTRA_FAIL,"):
            issues.append(f"line {line_no}: {line}")
        elif line.startswith("BENCH_INFO,"):
            for token in line.split(",")[1:]:
                if token.startswith("unit="):
                    timer = token[5:]
        elif line.startswith("BENCH_DONE,"):
            statuses["benchmark_runs_completed"] += 1
            for field in line.split(",")[1:]:
                if field.startswith("failed="):
                    try:
                        failures = int(field[7:])
                    except ValueError:
                        issues.append(f"line {line_no}: invalid failed field")
                    else:
                        if failures:
                            issues.append(f"line {line_no}: BENCH_DONE failures={failures}")
        elif line.startswith("BENCH,"):
            fields = line.split(",")
            if len(fields) != 14:
                issues.append(f"line {line_no}: malformed BENCH field count {len(fields)}")
                continue
            try:
                numbers = [int(n) for n in fields[2:]]
                item = Sample(fields[1], *numbers)
            except ValueError:
                issues.append(f"line {line_no}: invalid numeric BENCH field")
                continue
            if item.width <= 0 or item.loops <= 0:
                issues.append(f"line {line_no}: nonpositive width/loop count")
                continue
            samples.append(item)
    return samples, issues, statuses, timer, len(lines)


def analyze(lines: list[str], top: int = 20) -> tuple[int, list[Sample]]:
    samples, issues, statuses, timer, _ = parse(lines)
    print(f"Timer source: {timer}")
    if timer != "ee_cycles":
        print("NOTE: timer output is CLOCK TICKS, not measured EE CPU cycles.")
    print(
        f"Correctness PASS lines: {statuses['correctness_pass_lines']}; "
        f"extra PASS lines: {statuses['extra_pass_lines']}; "
        f"completed benchmark runs: {statuses['benchmark_runs_completed']}; "
        f"rows: {len(samples)}; failures: {len(issues)}"
    )
    if not statuses["correctness_pass_lines"]:
        issues.append("No correctness PASS line in the supplied log")
    if not statuses["extra_pass_lines"]:
        issues.append("No EXTRA_PASS line: forward/palette tests may not have run")
    if not statuses["benchmark_runs_completed"]:
        issues.append("No BENCH_DONE line: output may be incomplete")
    if not samples:
        issues.append("No benchmark rows found")
    elif not any(row.scalar_net > 0 and row.optimized_net > 0 for row in samples):
        issues.append("All measured net timings are zero; timer may be unavailable")
    for issue in issues[:30]:
        print(f"ERROR: {issue}", file=sys.stderr)

    # Compare competing kernels only under exactly matching test
    # conditions. Do not claim a single global winner across PNG types.
    groups: dict[tuple[int, int, int, int, int], list[Sample]] = defaultdict(list)
    for sample in samples:
        groups[(sample.filter_id, sample.bpp, sample.width,
                sample.row_align, sample.prev_align)].append(sample)

    winners = []
    for key, group in groups.items():
        usable = [row for row in group if row.speedup is not None]
        if usable:
            winners.append(min(usable, key=lambda row: row.optimized_net))
    winners.sort(key=lambda row: (row.filter_id, row.bpp,
                                  row.width, row.row_align, row.prev_align))
    print("\nPer-shape winners (first %d of %d):" % (top, len(winners)))
    print("filter bpp width align prev  variant                    scalar/net  gain")
    for row in winners[:top]:
        speedup = row.speedup
        print(f"{row.filter_id:6} {row.bpp:3} {row.width:5} "
              f"{row.row_align:5} {row.prev_align:4}  "
              f"{row.variant:25} {row.scalar_net:5}/{row.optimized_net:<5} "
              f"{speedup:.3f}x" if speedup else "")
    return (1 if issues else 0), samples


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", help="Captured PS2 console output, or - for stdin")
    parser.add_argument("--csv", help="Write all raw measurements to CSV")
    parser.add_argument("--top", type=int, default=20,
                        help="Number of per-shape winners to show")
    args = parser.parse_args(argv)
    if args.log == "-":
        lines = sys.stdin.read().splitlines()
    else:
        lines = Path(args.log).read_text(encoding="utf-8", errors="replace").splitlines()
    result, samples = analyze(lines, max(0, args.top))
    if args.csv:
        with open(args.csv, "w", newline="", encoding="utf-8") as handle:
            writer = csv.writer(handle)
            writer.writerow(list(Sample.__dataclass_fields__) + ["speedup"])
            for row in samples:
                writer.writerow([getattr(row, name) for name in
                                 Sample.__dataclass_fields__] + [row.speedup])
        print(f"Saved {len(samples)} raw benchmark samples to {args.csv}")
    return result


if __name__ == "__main__":
    raise SystemExit(main())
