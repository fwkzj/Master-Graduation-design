"""Where exactly do the hybrid's lost proofs go?

Stage 1 shows Hybrid proving 81/150 against CASH's 108/150. "It loses 27" is
not an explanation; this locates them. The useful split is by exit_reason --
`optimum` comes from CaDiCaL's complete search, `optimum_scip` from the ILP
solver's, and they are damaged by different things. If the loss sits entirely
in one bucket, that names the mechanism.

For each instance where CASH proved and Hybrid did not, this also reports how
far the hybrid got: a run that ended 0.1% from the optimum was nearly there,
one that ended 300% away was never in the race.
"""
import glob
import json
import re
from collections import Counter, defaultdict


def num(x):
    if isinstance(x, bool) or x is None:
        return None
    if isinstance(x, int):
        return x
    if isinstance(x, str) and re.fullmatch(r"-?\d+", x):
        return int(x)
    return None


def load(config):
    out = {}
    for path in glob.glob(f"runs/debug50/{config}/*/*/status.json"):
        d = json.load(open(path))
        inst = d["instance"].split("/")[-1].replace(".wcnf", "")
        r = d.get("record") or {}
        out[(inst, d["seed"])] = {
            "proved": bool(r.get("cash_proved_optimal")),
            "reason": r.get("exit_reason", "?"),
            "lb": num(r.get("final_lb")),
            "ub": num(r.get("final_ub")),
        }
    return out


cash = load("CASH")
hyb = load("Hybrid")

print("=== proof bucket split (CASH vs Hybrid) ===")
for name, table in (("CASH", cash), ("Hybrid", hyb)):
    c = Counter(v["reason"] for v in table.values())
    print(f'  {name:8s} ' + "  ".join(f"{k}={c[k]}" for k in sorted(c)))

keys = sorted(set(cash) & set(hyb))
print(f"\npaired on {len(keys)} (instance, seed) cells")

only_cash = [k for k in keys if cash[k]["proved"] and not hyb[k]["proved"]]
only_hyb = [k for k in keys if hyb[k]["proved"] and not cash[k]["proved"]]
both = [k for k in keys if cash[k]["proved"] and hyb[k]["proved"]]
neither = [k for k in keys if not cash[k]["proved"] and not hyb[k]["proved"]]
print(f"  proved by CASH only : {len(only_cash)}")
print(f"  proved by Hybrid only: {len(only_hyb)}")
print(f"  proved by both      : {len(both)}")
print(f"  proved by neither   : {len(neither)}")

print("\n=== the proofs CASH got and the hybrid did not ===")
print(f'{"instance":46s} {"seed":>8s} {"cash_reason":>14s} {"hyb_reason":>12s} '
      f'{"hyb_lb":>12s} {"hyb_ub":>12s} {"gap%":>9s}')
gaps = []
for k in sorted(only_cash):
    h = hyb[k]
    gap = ""
    if h["lb"] is not None and h["ub"]:
        g = 100.0 * (h["ub"] - h["lb"]) / abs(h["ub"])
        gaps.append(g)
        gap = f"{g:.2f}"
    print(f'{k[0][:46]:46s} {k[1]:>8} {cash[k]["reason"]:>14s} {h["reason"]:>12s} '
          f'{str(h["lb"]):>12s} {str(h["ub"]):>12s} {gap:>9s}')

if gaps:
    gaps.sort()
    print(f"\n  hybrid's optimality gap on those runs: "
          f"min {gaps[0]:.2f}%  median {gaps[len(gaps)//2]:.2f}%  max {gaps[-1]:.2f}%")

print("\n=== exit_reason transition for the lost proofs ===")
trans = Counter((cash[k]["reason"], hyb[k]["reason"]) for k in only_cash)
for (a, b), n in trans.most_common():
    print(f"  CASH {a:>12s} -> Hybrid {b:>12s} : {n}")

print("\n=== did any hybrid round even run on the lost instances? ===")
zero_rounds = 0
round_counts = []
for k in sorted(only_cash):
    ev = glob.glob(f"runs/debug50/Hybrid/{k[1]}/*/events.jsonl")
    match = None
    for p in ev:
        d = json.load(open(p.replace("events.jsonl", "status.json")))
        if d["instance"].split("/")[-1].replace(".wcnf", "") == k[0]:
            match = p
            break
    if match is None:
        continue
    n = sum(1 for line in open(match, errors="replace") if line.strip())
    round_counts.append(n)
    if n == 0:
        zero_rounds += 1
if round_counts:
    round_counts.sort()
    print(f"  rounds on those hybrid runs: min {round_counts[0]}  "
          f"median {round_counts[len(round_counts)//2]}  max {round_counts[-1]}")
    print(f"  runs where the handoff never happened (0 rounds): {zero_rounds}")
