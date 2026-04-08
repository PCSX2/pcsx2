#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, Iterable, List, Tuple


SECTOR_SIZE = 2048


@dataclass
class ReadAgg:
    sectors: int = 0
    runs: int = 0

    @property
    def bytes(self) -> int:
        return self.sectors * SECTOR_SIZE


def load_events(path: Path) -> Iterable[Dict[str, Any]]:
    with path.open("r", encoding="utf-8", errors="replace") as f:
        for line_no, line in enumerate(f, 1):
            text = line.strip()
            if not text:
                continue
            try:
                yield json.loads(text)
            except json.JSONDecodeError as e:
                print(f"[warn] skipped malformed JSON line {line_no}: {e}")


def summarize(path: Path) -> None:
    events = list(load_events(path))
    print(f"Trace file: {path}")
    print(f"Total events: {len(events)}")

    iso_maps = [e for e in events if e.get("type") == "iso_map"]
    if iso_maps:
        m = iso_maps[-1]
        print(
            "ISO map: files={files} assets_blt={assets} lsn={lsn} size={size}".format(
                files=m.get("files", 0),
                assets=m.get("has_assets_blt", False),
                lsn=m.get("assets_blt_lsn", 0),
                size=m.get("assets_blt_size", 0),
            )
        )

    ee_traces = [e for e in events if e.get("type") == "ee_trace"]
    pc_counts = Counter(int(e.get("pc", 0)) for e in ee_traces)
    if pc_counts:
        print("\nEE tracepoints hit:")
        for pc, count in pc_counts.most_common():
            print(f"  0x{pc:08x}: {count}")

    by_owner: Dict[str, ReadAgg] = defaultdict(ReadAgg)
    unknown_reads = 0
    for e in events:
        if e.get("type") != "iso_read":
            continue
        owner = str(e.get("owner", ""))
        sectors = int(e.get("sector_count", 0))
        if not owner:
            unknown_reads += sectors
            owner = "<unknown>"
        agg = by_owner[owner]
        agg.sectors += sectors
        agg.runs += 1

    if by_owner:
        print("\nTop ISO owners by streamed bytes:")
        ordered: List[Tuple[str, ReadAgg]] = sorted(by_owner.items(), key=lambda kv: kv[1].bytes, reverse=True)
        for owner, agg in ordered[:30]:
            print(f"  {owner:50}  runs={agg.runs:5d}  sectors={agg.sectors:7d}  bytes={agg.bytes:10d}")
        if unknown_reads:
            print(f"\nUnknown-sector total: {unknown_reads} ({unknown_reads * SECTOR_SIZE} bytes)")

    if ee_traces:
        print("\nSample EE string payloads:")
        shown = 0
        for e in ee_traces:
            a0s = str(e.get("a0_str", "")).strip()
            a1s = str(e.get("a1_str", "")).strip()
            if not a0s and not a1s:
                continue
            pc = int(e.get("pc", 0))
            print(f"  0x{pc:08x} a0='{a0s}' a1='{a1s}'")
            shown += 1
            if shown >= 20:
                break


def main() -> int:
    parser = argparse.ArgumentParser(description="Summarize SimpSkateTrace JSONL captured from instrumented PCSX2.")
    parser.add_argument("trace_jsonl", type=Path, help="Path to trace JSONL output")
    args = parser.parse_args()

    if not args.trace_jsonl.is_file():
        raise SystemExit(f"Trace file not found: {args.trace_jsonl}")

    summarize(args.trace_jsonl)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

