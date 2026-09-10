"""Compare the four configurations on the Stage 1 debug batch.

Reads the per-run status.json files the scheduler wrote, so it sees exactly the
records the runner produced rather than re-parsing logs.
"""
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

root = Path(sys.argv[1] if len(sys.argv) > 1 else
            "/home/fwkzj/HybridAlgorithm/runs/debug50")
CONFIGS = ["CASH", "Hybrid", "HybridNoInference", "SPB"]

rows = []
for status in root.glob("*/*/*/status.json"):
    try:
        s = json.loads(status.read_text())
    except Exception:
        continue
    rec = s.get("record")
    if not rec:
        continue
    rows.append({
        "instance": Path(s["instance"]).stem,
        "config": s["config"],
        "seed": s["seed"],
        "wall": s["wall_seconds"],
        "rounds": s["rounds"],
        "productive": s["productive_rounds"],
        "ub": rec.get("final_ub"),
        "lb": rec.get("final_lb"),
        "proved": bool(rec.get("cash_proved_optimal")),
        "scip": bool(rec.get("scip_proved_optimal")),
        "reason": rec.get("exit_reason"),
        "polls": rec.get("cash_deadline_polls"),
    })


def num(x):
    return int(x) if isinstance(x, str) and re.fullmatch(r"-?\d+", x) else None


per = defaultdict(lambda: defaultdict(list))
for r in rows:
    per[r["instance"]][r["config"]].append(r)

print(f"runs parsed: {len(rows)}")
print(f"instances seen: {len(per)}")
complete = sorted(i for i, c in per.items()
                  if all(len(c.get(cfg, [])) == 3 for cfg in CONFIGS))
print(f"instances complete across 4 configs x 3 seeds: {len(complete)}")
print()

hdr = (f'{"instance":40s} {"CASH":>9s} {"Hybrid":>9s} {"NoInf":>9s} '
       f'{"SPB":>9s} {"rnds":>5s} {"prod":>5s}  verdict')
print(hdr)
print("-" * len(hdr))


def best(cfg, key="ub"):
    v = [num(x[key]) for x in per_inst[cfg]]
    v = [x for x in v if x is not None]
    return min(v) if v else None


for inst in complete:
    per_inst = per[inst]
    rounds = sum(x["rounds"] for x in per_inst["Hybrid"])
    prod = sum(x["productive"] for x in per_inst["Hybrid"])
    cash, hyb = best("CASH"), best("Hybrid")
    if cash is None or hyb is None:
        verdict = "no ub"
    elif hyb < cash:
        verdict = "HYBRID BETTER"
    elif hyb > cash:
        verdict = "cash better"
    else:
        verdict = "tie"
    fmt = lambda v: "-" if v is None else str(v)
    print(f'{inst[:40]:40s} {fmt(cash):>9s} {fmt(hyb):>9s} '
          f'{fmt(best("HybridNoInference")):>9s} {fmt(best("SPB")):>9s} '
          f'{rounds:>5d} {prod:>5d}  {verdict}')

print()
print(f'{"config":20s} {"n":>4s} {"proved":>7s} {"via_scip":>9s} {"budget":>7s} '
      f'{"rounds":>7s} {"productive":>11s} {"med_wall":>9s}')
for cfg in CONFIGS:
    sub = [r for r in rows if r["config"] == cfg]
    if not sub:
        continue
    walls = sorted(r["wall"] for r in sub)
    med = walls[len(walls) // 2]
    print(f'{cfg:20s} {len(sub):>4d} {sum(1 for r in sub if r["proved"]):>7d} '
          f'{sum(1 for r in sub if r["scip"]):>9d} '
          f'{sum(1 for r in sub if r["reason"] == "budget"):>7d} '
          f'{sum(r["rounds"] for r in sub):>7d} '
          f'{sum(r["productive"] for r in sub):>11d} {med:>9.1f}')

print()
# The decisive question: on instances neither CASH-only nor Hybrid solves to
# optimality, does the SPB rounds ever improve on CASH's upper bound?
decided = []
for inst in complete:
    c, h, n = per[inst]["CASH"], per[inst]["Hybrid"], per[inst]["HybridNoInference"]
    if all(x["proved"] for x in c + h + n):
        continue
    cu = [num(x["ub"]) for x in c]
    hu = [num(x["ub"]) for x in h]
    cu = [x for x in cu if x is not None]
    hu = [x for x in hu if x is not None]
    if cu and hu:
        decided.append((inst, min(cu), min(hu), sum(x["rounds"] for x in h),
                        sum(x["productive"] for x in h)))

print(f"instances where at least one CASH-based run failed to prove optimality: "
      f"{len(decided)}")
for inst, c, h, r, p in sorted(decided, key=lambda t: t[1] - t[2]):
    flags = []
    if c != h:
        flags.append(f"cash={c} hybrid={h} delta={c - h}")
    if r == 0:
        flags.append("zero SPB rounds")
    elif p == 0:
        flags.append(f"{r} rounds, none productive")
    print(f"  {inst[:44]:44s} {'; '.join(flags) if flags else 'identical'}")
