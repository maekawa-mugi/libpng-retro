#!/usr/bin/env python3
"""Read a full one-boot PS2 libpng SPR experiment stdout log.

Require the original image/filter checks AND the independent SPR correctness
gate to pass. The displayed ratios compare identical kernels on RAM and SPR;
they are NOT speedups relative to generic scalar PNG processing.
"""
from __future__ import annotations
import argparse
import csv
import sys
from dataclasses import dataclass
from pathlib import Path

@dataclass(frozen=True)
class Row:
    kernel: str
    width: int
    mode: str
    ticks: int
    ratio: float
    status: str

def parse(lines: list[str]) -> tuple[list[Row], list[str], int]:
    rows: list[Row] = []
    issues: list[str] = []
    metadata = 0
    completed = 0
    declared_cases = -1
    for lineno, raw in enumerate(lines, 1):
        line = raw.strip()
        if line.startswith("SPR_META,"):
            metadata += 1
        elif line.startswith("SPR_FAIL,"):
            issues.append(f"line {lineno}: correctness failure: {line}")
        elif line.startswith("SPR_CASE,"):
            parts = line.split(",")
            if len(parts) != 8:
                issues.append(f"line {lineno}: malformed SPR_CASE")
                continue
            try:
                width, ticks, ratio = int(parts[2]), int(parts[4]), float(parts[5])
            except ValueError:
                issues.append(f"line {lineno}: invalid numeric SPR_CASE")
                continue
            if width <= 0 or ticks < 0 or ratio < 0 or parts[6] not in (
                    "MEASURED", "TIMER_NA") or parts[7] != "PASS":
                issues.append(f"line {lineno}: invalid SPR_CASE fields")
                continue
            rows.append(Row(parts[1], width, parts[3], ticks, ratio, parts[6]))
        elif line.startswith("SPR_RESULT,"):
            parts = line.split(",")
            if len(parts) >= 3 and parts[1] == "PASS":
                completed += 1
                for part in parts[2:]:
                    if part.startswith("cases="):
                        try:
                            declared_cases = int(part[6:])
                        except ValueError:
                            issues.append(f"line {lineno}: bad SPR_RESULT case count")
            else:
                issues.append(f"line {lineno}: unsuccessful SPR_RESULT: {line}")
        elif line.startswith("TEST: FAIL") or line.startswith("BENCH_FAIL,"):
            issues.append(f"line {lineno}: original correctness failure: {line}")
    if metadata != 1:
        issues.append(f"expected one SPR_META, got {metadata}")
    if completed != 1:
        issues.append(f"expected one SPR_RESULT,PASS, got {completed}")
    if not any("TEST: OK! code=0" in s for s in lines):
        issues.append("missing final TEST: OK! code=0")
    shapes: dict[tuple[str, int], set[str]] = {}
    for row in rows:
        shapes.setdefault((row.kernel, row.width), set()).add(row.mode)
    if declared_cases != len(shapes):
        issues.append(f"SPR_RESULT declares {declared_cases} shapes, found {len(shapes)}")
    for key, modes in shapes.items():
        if "ram" not in modes or "spr_row" not in modes or "spr_xfer" not in modes:
            issues.append(f"missing RAM / SPR row / inclusive transfer for {key}")
    return rows, issues, declared_cases

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("log", type=Path, help="complete unmodified PS2 stdout")
    ap.add_argument("--csv", type=Path, help="write machine-readable SPR measurements")
    ap.add_argument("--top", type=int, default=20, help="number of best SPR ratios")
    args = ap.parse_args()
    lines = args.log.read_text(encoding="utf-8", errors="replace").splitlines()
    rows, errors, count = parse(lines)
    for error in errors:
        print("ERROR:", error, file=sys.stderr)
    if errors:
        return 1
    if args.csv:
        with args.csv.open("w", newline="", encoding="utf-8") as output:
            wr = csv.writer(output)
            wr.writerow(("kernel", "width", "mode", "ticks", "ram_over_spr", "status"))
            for row in rows:
                wr.writerow((row.kernel, row.width, row.mode, row.ticks,
                             f"{row.ratio:.5f}", row.status))
    measured = [r for r in rows if r.status == "MEASURED" and r.mode != "ram"]
    winners = sorted(measured, key=lambda r: r.ratio, reverse=True)
    inclusive = [r for r in measured if r.mode == "spr_xfer" or
                 r.mode == "spr_row_xfer_lut_hot"]
    print(f"SPR correctness: PASS; {count} validated shapes; "
          f"{len(measured)} timed SPR comparisons")
    print("Raw ratios are RAM/SPR for the SAME optimized kernel.")
    print("A speed ratio above 1.00 means SPR wins that workload.")
    print(f"{'KERNEL':22} {'WIDTH':>6} {'MODE':24} {'RATIO':>9} {'TICKS':>12}")
    for r in winners[:max(args.top, 0)]:
        print(f"{r.kernel:22.22} {r.width:6} {r.mode:24.24} "
              f"{r.ratio:8.3f}x {r.ticks:12}")
    if inclusive:
        positives = sum(r.ratio > 1.0 for r in inclusive)
        print(f"Inclusive transfer comparisons above 1.00x: "
              f"{positives}/{len(inclusive)}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
