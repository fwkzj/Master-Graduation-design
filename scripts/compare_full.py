"""CASH vs Hybrid across the full MSE23W pool, one seed.

The batch summary already gives the headline (CASH 398/557, Hybrid 286/558), but
that comparison mixes two very different populations. The hybrid's clock starts
only after the instance is loaded, so a run CASH finishes inside the first
15-second window ends before any handoff happens -- for those runs the hybrid is
CASH plus a slower start, and nothing more. Splitting the runs by whether a
handoff ever occurred isolates the protocol's actual effect from its overhead.

Bounds on some instances run to hundreds of thousands of digits, so every
comparison here is done on the decimal string (length, then lexicographic)
rather than by int().
"""
import glob
import json
import re
from collections import Counter, defaultdict

BATCH = "runs/full2cfg"
BACKFILL = "runs/backfill_bloat"


def cmp_dec(a, b):
    """Three-way compare of two non-negative decimal integer strings."""
    a, b = str(a), str(b)
    if len(a) != len(b):
        return -1 if len(a) < len(b) else 1
    if a == b:
        return 0
    return -1 if a < b else 1


def num(x):
    """Bounds arrive as decimal strings or as bare ints; both are valid."""
    if isinstance(x, bool) or x is None:
        return None
    if isinstance(x, int):
        return str(x)
    if isinstance(x, str) and re.fullmatch(r"-?\d+", x):
        return x
    return None


def load():
    table = {}
    for root in (BATCH, BACKFILL):
        for path in glob.glob(f"{root}/*/*/*/status.json"):
            d = json.load(open(path))
            inst = d["instance"].split("/")[-1].replace(".wcnf", "")
            r = d.get("record") or {}
            table[(inst, d["config"])] = {
                "proved": bool(r.get("cash_proved_optimal")),
                "scip": bool(r.get("scip_proved_optimal")),
                "reason": r.get("exit_reason", "?"),
                "ub": num(r.get("final_ub")),
                "lb": num(r.get("final_lb")),
                "rounds": d.get("rounds", 0),
                "productive": d.get("productive_rounds", 0),
                "wall": d.get("wall_seconds"),
                "problems": d.get("problems") or [],
            }
    return table


t = load()
insts = sorted({k[0] for k in t})
paired = [i for i in insts if (i, "CASH") in t and (i, "Hybrid") in t]

print(f"instances with a CASH run    : {sum(1 for k in t if k[1] == 'CASH')}")
print(f"instances with a Hybrid run  : {sum(1 for k in t if k[1] == 'Hybrid')}")
print(f"paired on both               : {len(paired)}")
print()

print("=== headline: proofs ===")
for cfg in ("CASH", "Hybrid"):
    sub = [t[(i, cfg)] for i in paired]
    p = sum(1 for s in sub if s["proved"])
    print(f"  {cfg:8s} proved {p}/{len(paired)} ({100.0*p/len(paired):.1f}%)")

print()
print("=== paired outcome ===")
only_c = [i for i in paired if t[(i, "CASH")]["proved"] and not t[(i, "Hybrid")]["proved"]]
only_h = [i for i in paired if t[(i, "Hybrid")]["proved"] and not t[(i, "CASH")]["proved"]]
both = [i for i in paired if t[(i, "CASH")]["proved"] and t[(i, "Hybrid")]["proved"]]
neither = [i for i in paired if not t[(i, "CASH")]["proved"] and not t[(i, "Hybrid")]["proved"]]
print(f"  CASH only : {len(only_c)}")
print(f"  Hybrid only: {len(only_h)}")
print(f"  both      : {len(both)}")
print(f"  neither   : {len(neither)}")

print()
print("=== proof source (why each config proved) ===")
for cfg in ("CASH", "Hybrid"):
    c = Counter(t[(i, cfg)]["reason"] for i in paired)
    print(f'  {cfg:8s} ' + "  ".join(f"{k}={c[k]}" for k in sorted(c)))

print()
print("=== the isolating split: did a handoff ever happen? ===")
# `rounds` counts coordinator round events. Zero means CASH finished before the
# first window boundary, so the protocol never ran and the run is pure overhead.
print(f'{"group":34s} {"n":>5s} {"proved":>7s} {"rate":>7s}')
for label, sel in (
    ("Hybrid, 0 rounds (no handoff)", lambda i: t[(i, "Hybrid")]["rounds"] == 0),
    ("Hybrid, >=1 round (handoff)", lambda i: t[(i, "Hybrid")]["rounds"] > 0),
):
    group = [i for i in paired if sel(i)]
    p = sum(1 for i in group if t[(i, "Hybrid")]["proved"])
    rate = f"{100.0*p/len(group):.1f}%" if group else "-"
    print(f'{label:34s} {len(group):>5d} {p:>7d} {rate:>7s}')
# the matched CASH populations, to see whether the split itself selects for
# easy instances (it does: zero-round runs are the short ones)
for label, sel in (
    ("  ^ same instances, CASH", lambda i: t[(i, "Hybrid")]["rounds"] == 0),
    ("  ^ same instances, CASH", lambda i: t[(i, "Hybrid")]["rounds"] > 0),
):
    group = [i for i in paired if sel(i)]
    p = sum(1 for i in group if t[(i, "CASH")]["proved"])
    rate = f"{100.0*p/len(group):.1f}%" if group else "-"
    print(f'{label:34s} {len(group):>5d} {p:>7d} {rate:>7s}')

print()
print("=== where the proof loss lands, by CASH's own exit_reason ===")
trans = Counter()
for i in only_c:
    trans[(t[(i, "CASH")]["reason"], t[(i, "Hybrid")]["reason"])] += 1
for (a, b), n in trans.most_common():
    print(f"  CASH {a:>12s} -> Hybrid {b:>12s} : {n}")

print()
print("=== solution quality on the paired instances ===")
better = worse = equal = 0
quality_loss = []
for i in paired:
    c, h = t[(i, "CASH")]["ub"], t[(i, "Hybrid")]["ub"]
    if c is None or h is None:
        continue
    r = cmp_dec(h, c)
    if r < 0:
        better += 1
    elif r == 0:
        equal += 1
    else:
        worse += 1
        quality_loss.append(i)
print(f"  Hybrid UB better : {better}")
print(f"  Hybrid UB equal  : {equal}")
print(f"  Hybrid UB worse  : {worse}")

print()
print("=== runs with problems ===")
bad = [(k, v["problems"]) for k, v in t.items() if v["problems"]]
print(f"  {len(bad)}")
for k, p in bad[:5]:
    print(f"    {k}: {p}")
