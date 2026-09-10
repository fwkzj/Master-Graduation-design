"""SPB's gain against two different denominators.

`analyze_gain.py` measures SPB against `cash_ub_before_spb`, which is CASH's
bound *inside the windowed protocol*. But windowing itself degrades that bound
(see the control experiment: on causal_Link the windowed CASH never improves on
its initial bound at all, while uninterrupted CASH-only proves the optimum). So
a large "SPB gain" can mean either that SPB is strong or that the protocol left
CASH weak, and those are opposite conclusions about the design.

The honest denominator is CASH-only's final UB on the same instance and seed --
what the exact solver reaches when nothing interrupts it.
"""
import glob
import json
import re
from collections import defaultdict
from statistics import median

ROOT = "runs/debug50"


def num(x):
    if isinstance(x, bool) or x is None:
        return None
    if isinstance(x, int):
        return x
    if isinstance(x, str) and re.fullmatch(r"-?\d+", x):
        return int(x)
    return None


cash_only = {}
for path in glob.glob(f"{ROOT}/CASH/*/*/status.json"):
    d = json.load(open(path))
    r = d.get("record") or {}
    inst = d["instance"].split("/")[-1].replace(".wcnf", "")
    cash_only[(inst, d["seed"])] = {
        "ub": num(r.get("final_ub")), "proved": bool(r.get("cash_proved_optimal"))}


rows = []
for cfg in ("Hybrid", "HybridNoInference"):
    for ev in sorted(glob.glob(f"{ROOT}/{cfg}/*/*/events.jsonl")):
        st = ev.replace("events.jsonl", "status.json")
        try:
            d = json.load(open(st))
        except OSError:
            continue
        inst = d["instance"].split("/")[-1].replace(".wcnf", "")
        seed = d["seed"]
        rounds = []
        for line in open(ev, errors="replace"):
            if line.strip():
                try:
                    rounds.append(json.loads(line))
                except json.JSONDecodeError:
                    pass
        initial = next((e["cash_ub_before_spb"] for e in rounds
                        if e.get("cash_ub_before_spb", -1) >= 0), None)
        best_spb = min((e["spb_ub"] for e in rounds if e.get("spb_ub", -1) >= 0),
                       default=None)
        if initial is None or best_spb is None:
            continue
        co = cash_only.get((inst, seed))
        rows.append({
            "cfg": cfg, "inst": inst, "seed": seed, "initial": initial,
            "best_spb": best_spb, "cash_only": co["ub"] if co else None,
            "cash_only_proved": co["proved"] if co else None,
        })

print("=== SPB vs the two denominators ===")
print(f'{"instance":34s} {"seed":>8s} {"cfg":16s} {"cash windowed 1st":>17s} '
      f'{"SPB best":>14s} {"gain vs window%":>16s} {"CASH-only final":>16s} '
      f'{"gain vs CASH-only%":>18s}')
for r in sorted(rows, key=lambda r: r["inst"]):
    if r["cash_only"] is None:
        continue
    g1 = 100.0 * (r["initial"] - r["best_spb"]) / r["initial"]
    g2 = (100.0 * (r["cash_only"] - r["best_spb"]) / r["cash_only"]
          if r["cash_only"] else float("nan"))
    flag = "  <-- SPB still wins" if r["cash_only"] and r["best_spb"] < r["cash_only"] else ""
    print(f'{r["inst"][:34]:34s} {r["seed"]:>8} {r["cfg"]:16s} {r["initial"]:>17d} '
          f'{r["best_spb"]:>14d} {g1:>16.3f} {r["cash_only"]:>16d} {g2:>18.3f}{flag}')

print()
for cfg in ("Hybrid", "HybridNoInference"):
    sub = [r for r in rows if r["cfg"] == cfg and r["cash_only"]]
    if not sub:
        continue
    beats = [r for r in sub if r["best_spb"] < r["cash_only"]]
    g1 = [100.0 * (r["initial"] - r["best_spb"]) / r["initial"] for r in sub]
    g2 = [100.0 * (r["cash_only"] - r["best_spb"]) / r["cash_only"] for r in sub]
    print(f"--- {cfg}: {len(sub)} runs with both an SPB solution and a CASH-only counterpart")
    print(f"    SPB beats CASH's windowed first bound : "
          f"{sum(1 for r in sub if r['best_spb'] < r['initial'])}/{len(sub)}")
    print(f"    SPB beats CASH-only's final UB        : {len(beats)}/{len(sub)}")
    print(f"    median gain vs windowed bound         : {median(g1):.3f}%")
    print(f"    median gain vs CASH-only's final UB   : {median(g2):.3f}%")
    print(f"    CASH-only PROVED the optimum on       : "
          f"{sum(1 for r in sub if r['cash_only_proved'])}/{len(sub)} runs")
