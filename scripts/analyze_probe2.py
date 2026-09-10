"""Compare SPB's bound inside the protocol against SPB alone at the same cutoff.

The question: is the 3-second window too short, or is each round losing its
budget to per-round state rebuilding? Matching the cutoffs separates the two --
if hybrid@3s is far worse than standalone@3s, the rounds are paying an overhead
the standalone run does not; if they match, the window is simply short and a
longer one should help proportionally.
"""
import glob
import json
import re
from collections import defaultdict


def num(x):
    if isinstance(x, bool) or x is None:
        return None
    if isinstance(x, int):
        return x
    if isinstance(x, str) and re.fullmatch(r"-?\d+", x):
        return int(x)
    return None


def record(path):
    try:
        text = open(path, errors="replace").read()
    except OSError:
        return None
    m = re.findall(r'\{"event":"run_complete".*\}', text)
    if not m:
        return None
    try:
        return json.loads(m[-1])
    except json.JSONDecodeError:
        return None


hyb = defaultdict(dict)
spb = defaultdict(dict)
for path in glob.glob("runs/probe2/h_*.jsonl"):
    m = re.search(r"h_(.+)_w(\d+)\.jsonl", path)
    inst, w = m.group(1), int(m.group(2))
    rounds = []
    for line in open(path, errors="replace"):
        if line.strip():
            try:
                rounds.append(json.loads(line))
            except json.JSONDecodeError:
                pass
    vals = [e["spb_ub"] for e in rounds if e.get("spb_ub", -1) >= 0]
    hyb[inst][w] = {
        "rounds": len(rounds),
        "with_sol": len(vals),
        "best_spb": min(vals) if vals else None,
        "first_spb": vals[0] if vals else None,
    }
for path in glob.glob("runs/probe2/s_*.jsonl"):
    m = re.search(r"s_(.+)_t(\d+)\.jsonl", path)
    inst, t = m.group(1), int(m.group(2))
    spb[inst][t] = record(path.replace(".jsonl", ".out"))

for inst in sorted(hyb):
    print("=" * 78)
    print(inst)
    print(f'  {"hybrid: spb-window":22s} {"rounds":>7s} {"w/ sol":>7s} '
          f'{"1st spb_ub":>16s} {"best spb_ub":>16s}')
    for w in sorted(hyb[inst]):
        h = hyb[inst][w]
        f = lambda v: "-" if v is None else str(v)
        print(f'  {w:>3d}s {"":17s} {h["rounds"]:>7d} {h["with_sol"]:>7d} '
              f'{f(h["first_spb"]):>16s} {f(h["best_spb"]):>16s}')
    print()
    print(f'  {"SPB standalone":22s} {"cutoff":>7s} {"runtime":>8s} {"final_ub":>16s}')
    for t in sorted(spb[inst]):
        r = spb[inst][t] or {}
        print(f'  {"":22s} {t:>6d}s {str(r.get("wall_seconds")):>8s} '
              f'{str(r.get("final_ub")):>16s}')
    print()
