"""Per-instance CASH-vs-Hybrid comparison for the Stage 1 batch.

Two things this adds over a naive summary read:

1. `scip_seconds == 0` in a run_complete record is impossible on the real code
   path (apply_scip_limit always returns at least the cash window, 15), so it
   marks a record written by the budget watchdog. Those records carry default
   `cash_deadline_polls`/`spb_certificate_ub` and, for the hybrid, skip the
   SPB-certificate backfill that a graceful stop applies -- so their `final_ub`
   is not comparable with a graceful run's and must be counted separately.
2. An instance only carries information about the SPB component when CASH did
   NOT prove it optimal. Where CASH proves optimality inside the first window
   the two configurations run identical code and agree bit for bit.
"""
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

root = Path(sys.argv[1] if len(sys.argv) > 1 else "/home/fwkzj/HybridAlgorithm/runs/debug50")
CONFIGS = ["CASH", "Hybrid", "HybridNoInference", "SPB"]


def num(x):
    return int(x) if isinstance(x, str) and re.fullmatch(r"-?\d+", x) else None


rows = defaultdict(lambda: defaultdict(list))
watchdog = Counter()
for status_file in root.glob("*/*/*/status.json"):
    s = json.loads(status_file.read_text())
    rec = s.get("record") or {}
    if not rec:
        continue
    wd = rec.get("scip_seconds") in (0, "0") and s["config"] in ("CASH", "Hybrid", "HybridNoInference")
    if wd:
        watchdog[s["config"]] += 1
    rows[Path(s["instance"]).stem][s["config"]].append({
        "ub": num(rec.get("final_ub")),
        "proved": bool(rec.get("cash_proved_optimal")),
        "ub_as_recorded": num(rec.get("final_ub")),
        "watchdog": wd,
        "reason": rec.get("exit_reason"),
        "rounds": s["rounds"],
        "productive": s["productive_rounds"],
        "wall": s["wall_seconds"],
    })

complete = sorted(i for i, c in rows.items()
                  if all(len(c.get(cfg, [])) == 3 for cfg in CONFIGS))

print(f"parsed {sum(len(v) for c in rows.values() for v in c.values())} runs, "
      f"{len(rows)} instances, {len(complete)} complete across 4 configs x 3 seeds")
print(f"watchdog-written CASH-based records (scip_seconds==0): {dict(watchdog)}")
print()

hdr = (f'{"instance":34s} | {"CASH":>22s} | {"Hybrid":>22s} | {"HNI":>22s} | verdict')
print(hdr)
print("-" * len(hdr))


def cell(runs):
    best = [r["ub"] for r in runs if r["ub"] is not None]
    proved = sum(1 for r in runs if r["proved"])
    wd = sum(1 for r in runs if r["watchdog"])
    b = min(best) if best else None
    tag = f"{b}" if b is not None else "-"
    if proved:
        tag += f" opt{proved}"
    if wd:
        tag += f" WD{wd}"
    return tag


classes = Counter()
for inst in complete:
    c = rows[inst]
    cells = [cell(c[cfg]) for cfg in CONFIGS[:3]]
    cash_proved = sum(1 for r in c["CASH"] if r["proved"])
    hyb_proved = sum(1 for r in c["Hybrid"] if r["proved"])
    cu = min([r["ub"] for r in c["CASH"] if r["ub"] is not None], default=None)
    hu = min([r["ub"] for r in c["Hybrid"] if r["ub"] is not None], default=None)
    if cash_proved == 3 and hyb_proved == 3 and cu == hu:
        verdict = "identical (CASH proves in window 1)"
        classes["identical"] += 1
    elif cu is not None and hu is not None and hu < cu:
        verdict = f"HYBRID BETTER by {cu - hu}"
        classes["hybrid_better"] += 1
    else:
        verdict = "CASH BETTER" + (f" by {hu - cu}" if cu is not None and hu is not None else "")
        classes["cash_better"] += 1
    print(f'{inst[:34]:34s} | {cells[0]:>22s} | {cells[1]:>22s} | {cells[2]:>22s} | {verdict}')

print()
print("classes:", dict(classes))
print()
print("informative instances (CASH does not prove all 3 seeds):")
for inst in complete:
    c = rows[inst]
    if sum(1 for r in c["CASH"] if r["proved"]) == 3:
        continue
    for cfg in CONFIGS:
        runs = c[cfg]
        reasons = Counter(r["reason"] for r in runs)
        walls = [r["wall"] for r in runs]
        print(f'  {inst[:40]:40s} {cfg:18s} ub={cell(runs):>24s} '
              f'wall={min(walls):.0f}-{max(walls):.0f}s {dict(reasons)}')
