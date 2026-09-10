"""Per-round verdict for the hybrid configs: did CASH actually take SPB's bound?

`final_ub` on a hybrid run is not by itself proof that the handoff helped -- the
runner backfills it from SPB's certificate when CASH ended holding no model. The
event log is the only place that records whether CASH accepted the candidate, so
the comparison is done from the events, not the summary.
"""
import json
import sys
from collections import Counter
from pathlib import Path

root = Path(sys.argv[1] if len(sys.argv) > 1 else "/home/fwkzj/HybridAlgorithm/runs/debug50")

print(f'{"instance":38s} {"cfg":18s} {"sd":>2s} {"rounds":>6s} {"prod":>5s} '
      f'{"accept":>6s} {"impr":>5s} {"ub_before":>12s} {"ub_after":>12s} {"spb_best":>12s}')
for cfg in ("Hybrid", "HybridNoInference"):
    for run_dir in sorted((root / cfg).glob("*/*")):
        ev = run_dir / "events.jsonl"
        if not ev.exists():
            continue
        rounds = prod = accept = improved = 0
        first_ub = last_after = None
        spb_best = None
        for line in ev.read_text(errors="replace").splitlines():
            if not line.strip():
                continue
            try:
                e = json.loads(line)
            except json.JSONDecodeError:
                continue
            rounds += 1
            if e.get("spb_ub", -1) >= 0:
                prod += 1
                spb_best = e["spb_ub"] if spb_best is None else min(spb_best, e["spb_ub"])
            if first_ub is None:
                first_ub = e.get("cash_ub_before_spb")
            if e.get("cash_bound_result") == "accepted":
                accept += 1
                after = e.get("cash_ub_after_spb", -1)
                before = e.get("cash_ub_before_spb", -1)
                last_after = after
                if before < 0 or (after >= 0 and after < before):
                    improved += 1
        if rounds == 0:
            continue
        fmt = lambda v: "-" if v in (None, -1) else str(v)
        print(f'{run_dir.parent.parent.name[:38]:38s} {cfg:18s} '
              f'{run_dir.parent.name:>2s} {rounds:>6d} {prod:>5d} {accept:>6d} '
              f'{improved:>5d} {fmt(first_ub):>12s} {fmt(last_after):>12s} '
              f'{fmt(spb_best):>12s}')

print()
for cfg in ("Hybrid", "HybridNoInference"):
    total = Counter()
    for run_dir in sorted((root / cfg).glob("*/*")):
        ev = run_dir / "events.jsonl"
        if not ev.exists():
            continue
        for line in ev.read_text(errors="replace").splitlines():
            if not line.strip():
                continue
            try:
                e = json.loads(line)
            except json.JSONDecodeError:
                continue
            total[e.get("cash_bound_result", "?")] += 1
    print(f"{cfg}: {dict(total)}")
