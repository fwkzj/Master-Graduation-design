"""Stage 1 status: the frb instance that used to fail, plus the run-wide picture."""
import glob
import json
import sys
from collections import Counter, defaultdict

root = sys.argv[1] if len(sys.argv) > 1 else "runs/debug50"
target = sys.argv[2] if len(sys.argv) > 2 else "frb_wt-frb25-13-1"

print(f"=========== {target}: every run ===========")
found = 0
for cfg in ("CASH", "SPB", "Hybrid", "HybridNoInference"):
    for seed in ("20260909", "20260910", "20260911"):
        path = f"{root}/{cfg}/{seed}/{target}/status.json"
        try:
            d = json.load(open(path))
        except OSError:
            print(f"  {cfg:18s} {seed}  MISSING")
            continue
        found += 1
        r = d.get("record") or {}
        print(f"  {cfg:18s} {seed}  exit={d['exit_code']:3d} "
              f"wall={d['wall_seconds']:6.1f}s "
              f"reason={str(r.get('exit_reason')):14s} "
              f"lb={str(r.get('final_lb')):>7s} ub={str(r.get('final_ub')):>7s} "
              f"proved={str(r.get('cash_proved_optimal')):5s} "
              f"scip={str(r.get('scip_proved_optimal')):5s} "
              f"problems={d['problems']}")
bad_exit = 0
bad_problems = 0
for cfg in ("CASH", "SPB", "Hybrid", "HybridNoInference"):
    for seed in ("20260909", "20260910", "20260911"):
        try:
            d = json.load(open(f"{root}/{cfg}/{seed}/{target}/status.json"))
        except OSError:
            continue
        if d["exit_code"] != 0:
            bad_exit += 1
        if d["problems"]:
            bad_problems += 1
print(f"  --> present {found}/12, non-zero exit: {bad_exit}, "
      f"with problems: {bad_problems}")

print()
print("=========== run-wide exit_reason / exit code ===========")
reasons = Counter()
codes = Counter()
walls = defaultdict(list)
proved = Counter()
by_cfg = defaultdict(Counter)
n = 0
for path in glob.glob(f"{root}/*/*/*/status.json"):
    d = json.load(open(path))
    cfg = d["config"]
    codes[d["exit_code"]] += 1
    n += 1
    r = d.get("record") or {}
    reason = r.get("exit_reason", "<no record>")
    reasons[reason] += 1
    by_cfg[cfg][reason] += 1
    walls[cfg].append(d["wall_seconds"])
    if r.get("cash_proved_optimal"):
        proved[cfg] += 1
    if r.get("scip_proved_optimal"):
        proved[cfg + "/via_scip"] += 1

print(f"runs: {n}")
print(f"exit codes: {dict(codes)}")
print(f"exit_reason: {dict(reasons)}")
print()
print(f'{"config":20s} {"n":>4s} {"proved":>7s} {"via_scip":>9s} {"med_wall":>9s}  reasons')
for cfg in ("CASH", "SPB", "Hybrid", "HybridNoInference"):
    if cfg not in walls:
        continue
    w = sorted(walls[cfg])
    print(f'{cfg:20s} {len(w):>4d} {proved[cfg]:>7d} {proved[cfg + "/via_scip"]:>9d} '
          f'{w[len(w)//2]:>9.1f}  {dict(by_cfg[cfg])}')
