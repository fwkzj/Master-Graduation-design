"""How much do the watchdog-written records distort Stage 1?

Defect six (docs/experiments/2026-09-10-debug-log.md §10) makes the watchdog
publish a record built from a default-constructed RunSummary. Two consequences
are measurable from the artefacts alone:

  * the record's final_ub is CASH's UB at kill time, and the SPB-certificate
    backfill (hybrid_main.cpp:600-616) never runs, so any SPB bound better than
    CASH's is silently dropped;
  * cash_proved_optimal is always false and exit_reason is "budget".

`scip_seconds == 0` identifies these records: on the real path
apply_scip_limit() always returns at least the CASH window, so zero is
impossible there.
"""
import glob
import json
import re
from statistics import median


def num(x):
    if isinstance(x, bool) or x is None:
        return None
    if isinstance(x, int):
        return x
    if isinstance(x, str) and re.fullmatch(r"-?\d+", x):
        return int(x)
    return None


rows = []
for path in glob.glob("runs/debug50/*/*/*/status.json"):
    d = json.load(open(path))
    r = d.get("record") or {}
    if not (num(r.get("scip_seconds")) == 0 and r.get("exit_reason") == "budget"):
        continue
    events = path.replace("status.json", "events.jsonl")
    best_spb = None
    rounds = 0
    for line in open(events, errors="replace"):
        if not line.strip():
            continue
        try:
            e = json.loads(line)
        except json.JSONDecodeError:
            continue
        rounds += 1
        v = num(e.get("spb_ub"))
        if v is not None and v >= 0:
            best_spb = v if best_spb is None else min(best_spb, v)
    rows.append({
        "cfg": d["config"],
        "inst": d["instance"].split("/")[-1].replace(".wcnf", ""),
        "seed": d["seed"],
        "wall": d.get("wall_seconds"),
        "rounds": rounds,
        "recorded_ub": num(r.get("final_ub")),
        "recorded_lb": num(r.get("final_lb")),
        "best_spb": best_spb,
    })

print(f"watchdog-written records: {len(rows)}")
print()
print(f'{"config":20s} {"instance":50s} {"seed":>8s} {"wall":>7s} {"rnds":>5s} '
      f'{"recorded_ub":>13s} {"best_spb":>13s} {"understated":>11s}')
gaps = []
for r in sorted(rows, key=lambda r: (r["cfg"], r["inst"])):
    gap = ""
    if r["best_spb"] is not None and r["recorded_ub"] and r["best_spb"] < r["recorded_ub"]:
        p = 100.0 * (r["recorded_ub"] - r["best_spb"]) / r["recorded_ub"]
        gaps.append(p)
        gap = f"{p:.2f}%"
    print(f'{r["cfg"]:20s} {r["inst"][:50]:50s} {r["seed"]:>8} '
          f'{str(r["wall"]):>7s} {r["rounds"]:>5d} {str(r["recorded_ub"]):>13s} '
          f'{str(r["best_spb"]):>13s} {gap:>11s}')

print()
print(f"records whose UB is worse than an SPB bound already returned: {len(gaps)}")
if gaps:
    print(f"  median understatement {median(gaps):.2f}%   max {max(gaps):.2f}%")
no_rounds = [r for r in rows if r["rounds"] == 0]
print(f"records with zero rounds recorded: {len(no_rounds)} "
      f"(events lost entirely, no SPB data recoverable)")
