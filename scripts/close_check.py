"""For the 57 watchdog records with near-matching bounds, could a proof have
been imminent?

A run on the verge of proving optimality must already hold the optimal UB --
proving is closing the last gap to a value it has already found, so if its UB is
worse than CASH-only's proven optimum, no proof was reachable at that instant,
regardless of how close lb and ub look relative to each other.

This compares each watchdog record's UB against CASH-only's UB on the same
instance. Where CASH-only proved, its UB *is* the optimum.
"""
import glob
import json


def cmp_dec(a, b):
    a, b = str(a), str(b)
    if len(a) != len(b):
        return -1 if len(a) < len(b) else 1
    return 0 if a == b else (-1 if a < b else 1)


def num(x):
    if isinstance(x, bool) or x is None:
        return None
    if isinstance(x, int):
        return str(x)
    if isinstance(x, str) and x.isdigit():
        return x
    return None


cash = {}
for path in glob.glob("runs/full2cfg/CASH/*/*/status.json"):
    d = json.load(open(path))
    inst = d["instance"].split("/")[-1].replace(".wcnf", "")
    r = d.get("record") or {}
    cash[inst] = {
        "ub": num(r.get("final_ub")),
        "proved": bool(r.get("cash_proved_optimal")),
    }

held_optimum = 0
worse = 0
no_cash = 0
rows = []
for path in glob.glob("runs/full2cfg/Hybrid/*/*/status.json"):
    d = json.load(open(path))
    if d.get("rounds", 0) == 0:
        continue
    r = d.get("record") or {}
    if not (r.get("scip_seconds") == 0 and r.get("exit_reason") == "budget"):
        continue
    inst = d["instance"].split("/")[-1].replace(".wcnf", "")
    hub = num(r.get("final_ub"))
    c = cash.get(inst)
    if c is None or c["ub"] is None or hub is None:
        no_cash += 1
        continue
    if cmp_dec(hub, c["ub"]) == 0:
        held_optimum += 1
        rows.append((inst, hub, c["ub"], c["proved"], "HELD the optimum"))
    else:
        worse += 1
        rows.append((inst, hub, c["ub"], c["proved"], "UB worse than CASH-only"))

print(f"watchdog records among handoff runs: {held_optimum + worse + no_cash}")
print(f"  held CASH-only's UB exactly           : {held_optimum}")
print(f"  UB strictly worse than CASH-only's    : {worse}")
print(f"  no comparable CASH run                : {no_cash}")
print()
print("A run whose UB is worse than the optimum cannot prove optimality, so only")
print("the first group could possibly have been cut short of a proof.")
print()
for inst, hub, cub, proved, verdict in sorted(rows):
    if verdict.startswith("HELD"):
        print(f'  {inst[:56]:56s} cash_proved={proved}  {verdict}')
