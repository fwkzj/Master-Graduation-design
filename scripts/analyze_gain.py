"""How much does SPB improve the exact solver's upper bound, per round and per run?

Every `spb_round` event already carries both numbers, so the comparison is exact
rather than reconstructed: `cash_ub_before_spb` is what CASH held when SPB was
called, `spb_ub` is what SPB returned, `cash_ub_after_spb` is what CASH held
afterwards. A `-1` bound means "unknown / no solution", not zero.

Reported two ways because they answer different questions:
  per round -- when SPB does improve, by how much, relative to CASH's bound then
  per run   -- from CASH's first bound to SPB's best, i.e. SPB's total
               contribution to the run's answer quality
"""
import glob
import json
from collections import defaultdict
from statistics import median

ROOT = "runs/debug50"


def pct(values, p):
    if not values:
        return None
    s = sorted(values)
    k = min(len(s) - 1, max(0, int(round((p / 100.0) * (len(s) - 1)))))
    return s[k]


round_gains = defaultdict(list)      # relative gain, rounds where SPB won
round_losses = defaultdict(list)     # relative loss, rounds where SPB lost
stats = defaultdict(lambda: defaultdict(int))
per_run = []

for cfg in ("Hybrid", "HybridNoInference"):
    for ev in sorted(glob.glob(f"{ROOT}/{cfg}/*/*/events.jsonl")):
        inst = ev.split("/")[-2]
        seed = ev.split("/")[-3]
        rounds = []
        for line in open(ev, errors="replace"):
            if not line.strip():
                continue
            try:
                rounds.append(json.loads(line))
            except json.JSONDecodeError:
                pass
        if not rounds:
            continue

        s = stats[cfg]
        initial = None
        for e in rounds:
            before = e.get("cash_ub_before_spb", -1)
            spb = e.get("spb_ub", -1)
            if initial is None and before is not None and before >= 0:
                initial = before
            if spb is None or spb < 0:
                s["rounds_no_spb_solution"] += 1
                continue
            s["rounds_with_spb_solution"] += 1
            if before is None or before < 0:
                s["rounds_cash_ub_unknown"] += 1
                continue
            s["rounds_comparable"] += 1
            if spb < before:
                s["rounds_better"] += 1
                round_gains[cfg].append((before - spb) / float(before))
            elif spb == before:
                s["rounds_equal"] += 1
            else:
                s["rounds_worse"] += 1
                round_losses[cfg].append((spb - before) / float(before))

        best_spb = min((e["spb_ub"] for e in rounds
                        if e.get("spb_ub", -1) >= 0), default=None)
        final = rounds[-1].get("cash_ub_after_spb", -1)
        if final is None or final < 0:
            final = rounds[-1].get("cash_ub_before_spb", -1)
        if initial and best_spb is not None:
            per_run.append({
                "cfg": cfg, "inst": inst, "seed": seed,
                "rounds": len(rounds), "initial": initial, "best_spb": best_spb,
                "final": final,
                "spb_gain": (initial - best_spb) / float(initial),
                "end_gain": ((initial - final) / float(initial)
                             if final and final > 0 else None),
            })

for cfg in ("Hybrid", "HybridNoInference"):
    s = stats[cfg]
    total = s["rounds_no_spb_solution"] + s["rounds_with_spb_solution"]
    n_sol = s["rounds_with_spb_solution"]
    print(f"=== {cfg}: per-round ===")
    print(f"  rounds total                : {total}")
    print(f"  SPB returned no solution    : {s['rounds_no_spb_solution']}")
    print(f"  SPB returned a solution     : {n_sol} "
          f"({100.0*n_sol/total:.1f}% of rounds)")
    print(f"    CASH's bound unknown (-1) : {s['rounds_cash_ub_unknown']}")
    print(f"    comparable                : {s['rounds_comparable']}")
    print(f"      SPB better              : {s['rounds_better']}")
    print(f"      equal                   : {s['rounds_equal']}")
    print(f"      worse                   : {s['rounds_worse']}")
    g = round_gains[cfg]
    l = round_losses[cfg]
    if n_sol:
        print(f"    better/equal/worse as % of SPB solutions: "
              f"{100.0*s['rounds_better']/n_sol:.1f} / "
              f"{100.0*s['rounds_equal']/n_sol:.1f} / "
              f"{100.0*s['rounds_worse']/n_sol:.1f}")
    if g:
        print(f"  improvement when it wins (%): "
              f"median {100*median(g):.3f}  p25 {100*pct(g,25):.3f}  "
              f"p75 {100*pct(g,75):.3f}  max {100*max(g):.3f}")
    if l:
        print(f"  regression when it loses (%): "
              f"median {100*median(l):.3f}  max {100*max(l):.3f}")
    print()

print("=== per-run: SPB's total contribution (relative to CASH's first bound) ===")
print(f'{"instance":38s} {"seed":>8s} {"cfg":16s} {"rnds":>4s} {"initial":>18s} '
      f'{"best_spb":>18s} {"SPB gain%":>9s} {"end gain%":>9s}')
rows = [r for r in per_run if r["spb_gain"] > 0]
for r in sorted(rows, key=lambda r: -r["spb_gain"]):
    print(f'{r["inst"][:38]:38s} {r["seed"]:>8} {r["cfg"]:16s} {r["rounds"]:>4d} '
          f'{r["initial"]:>18d} {r["best_spb"]:>18d} {100*r["spb_gain"]:>9.3f} '
          f'{(100*r["end_gain"] if r["end_gain"] is not None else float("nan")):>9.3f}')

print()
allg = [r["spb_gain"] for r in per_run]
pos = [r for r in per_run if r["spb_gain"] > 0]
print(f"hybrid runs measured            : {len(allg)}")
print(f"...where SPB improved anything  : {len(pos)} "
      f"({100.0*len(pos)/len(allg):.1f}%)")
if pos:
    print(f"...median improvement           : {100*median([r['spb_gain'] for r in pos]):.3f}%")
    print(f"...max improvement              : {100*max(r['spb_gain'] for r in pos):.3f}%")
    print(f"...fewer than 1% better         : "
          f"{sum(1 for r in pos if r['spb_gain'] < 0.01)}")
    print(f"...better by 10% or more        : "
          f"{sum(1 for r in pos if r['spb_gain'] >= 0.10)}")
