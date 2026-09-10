"""Was SPB's improvement internalised by CASH, or only reported?

A round CASH accepts sets its *own* upper bound, so the improvement survives in
the solver's answer. A run whose final UB comes only from the certificate
backfill (hybrid_main.cpp:611-613) reports SPB's number while CASH itself still
holds the worse bound -- a different claim entirely, and the one that decides
whether "SPB improved the UB" is a statement about the solver or about the
reporting.
"""
import glob
import json
import re
from collections import Counter, defaultdict

ROOT = "runs/debug50"


def num(x):
    if isinstance(x, bool) or x is None:
        return None
    if isinstance(x, int):
        return x
    if isinstance(x, str) and re.fullmatch(r"-?\d+", x):
        return int(x)
    return None


def accepted_rounds(cfg, seed, inst):
    """How many rounds CASH accepted, and the best SPB bound it ever saw."""
    path = f"{ROOT}/{cfg}/{seed}/{inst}/events.jsonl"
    acc = 0
    seen = 0
    best = None
    try:
        lines = open(path, errors="replace").read().splitlines()
    except OSError:
        return None, None, None
    for line in lines:
        if not line.strip():
            continue
        try:
            e = json.loads(line)
        except json.JSONDecodeError:
            continue
        seen += 1
        if e.get("cash_bound_result") == "accepted":
            acc += 1
        s = e.get("spb_ub", -1)
        if s is not None and s >= 0:
            best = s if best is None else min(best, s)
    return acc, seen, best


runs = defaultdict(dict)
for path in glob.glob(f"{ROOT}/*/*/*/status.json"):
    d = json.load(open(path))
    r = d.get("record") or {}
    if not r:
        continue
    inst = d["instance"].split("/")[-1].replace(".wcnf", "")
    runs[(inst, d["seed"])][d["config"]] = {
        "ub": num(r.get("final_ub")),
        "inc": num(r.get("final_incumbent")),
        "proved": bool(r.get("cash_proved_optimal")),
        "watchdog": r.get("scip_seconds") in (0, "0"),
        "cert": num(r.get("spb_certificate_ub")),
    }

print("=== runs where a hybrid config beat CASH-only on the same instance+seed ===")
print(f'{"instance":36s} {"seed":>8s} {"cfg":18s} {"cash-only":>13s} {"hybrid":>13s} '
      f'{"accepted":>8s} {"rounds":>6s} {"best_spb":>13s}  internalised?')
tally = Counter()
for (inst, seed), cfgmap in sorted(runs.items()):
    cash = cfgmap.get("CASH")
    if not cash or cash["ub"] is None:
        continue
    for cfg in ("Hybrid", "HybridNoInference"):
        h = cfgmap.get(cfg)
        if not h or h["ub"] is None:
            continue
        if h["ub"] >= cash["ub"]:
            continue
        acc, seen, best = accepted_rounds(cfg, seed, inst)
        internal = acc is not None and acc > 0
        tally[("accepted" if internal else "certificate only", cfg)] += 1
        print(f"{inst[:36]:36s} {seed:>8} {cfg:18s} {cash['ub']:>13d} {h['ub']:>13d} "
              f"{str(acc):>8s} {str(seen):>6s} {str(best):>13s}  "
              f"{'YES' if internal else 'NO'}")

print()
print("=== tally (hybrid beats CASH-only) ===")
for k, v in tally.most_common():
    print(f"  {k[0]:18s} {k[1]:18s} {v}")

print()
print("=== proof impact, all configs ===")
for cfg in ("CASH", "Hybrid", "HybridNoInference", "SPB"):
    sub = [v[cfg] for v in runs.values() if cfg in v]
    n = len(sub)
    p = sum(1 for s in sub if s["proved"])
    print(f"  {cfg:18s} n={n:3d} proved={p:3d} ({100.0*p/n:.0f}%)")

print()
print("=== paired: proved by CASH-only vs by Hybrid, same instance+seed ===")
both = Counter()
for (inst, seed), cfgmap in runs.items():
    c, h = cfgmap.get("CASH"), cfgmap.get("Hybrid")
    if not c or not h:
        continue
    both[(bool(c["proved"]), bool(h["proved"]))] += 1
for k, v in sorted(both.items()):
    label = {True: "proved", False: "not proved"}
    print(f"  CASH-only {label[k[0]]:11s} / Hybrid {label[k[1]]:11s}: {v}")
