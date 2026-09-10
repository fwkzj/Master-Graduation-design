"""Could the watchdog records hide a proof among the 265 handoff runs?

Defect six builds the watchdog's record from a default-constructed RunSummary,
so `cash_proved_optimal` is false in those records no matter what the solver
held. 100 of the 265 handoff runs are such records, so "0 proved" is partly a
statement about the recorder rather than about the search.

The masking window is small: the coordinator's clock starts after parsing and
the watchdog fires at budget+3, so only a run whose parse exceeds 3 seconds can
be cut short, and only by (parse-3) seconds. A proof in that final sliver is
possible but requires the optimality gap to be closing at the end of the
budget. This measures the gap those runs actually ended with -- if the bounds
are orders of magnitude apart, no proof was imminent and the 0 stands.
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


rows = []
for path in glob.glob("runs/full2cfg/Hybrid/*/*/status.json"):
    d = json.load(open(path))
    if d.get("rounds", 0) == 0:
        continue
    r = d.get("record") or {}
    if not (r.get("scip_seconds") == 0 and r.get("exit_reason") == "budget"):
        continue
    lb, ub = num(r.get("final_lb")), num(r.get("final_ub"))
    rows.append((d["instance"].split("/")[-1].replace(".wcnf", ""), lb, ub,
                 d.get("wall_seconds")))

print(f"watchdog records among handoff runs: {len(rows)}")
print()

closed = 0           # bounds identical -> would have been a proof
narrow = []          # within one digit length, i.e. genuinely close
print(f'{"instance":52s} {"wall":>7s}  state')
for inst, lb, ub, wall in sorted(rows):
    if lb is None or ub is None:
        state = "no bounds recorded"
    else:
        c = cmp_dec(lb, ub)
        if c == 0:
            closed += 1
            state = "lb == ub  <-- WOULD BE A PROOF"
        elif abs(len(lb) - len(ub)) <= 1:
            narrow.append(inst)
            state = f"close (digit lengths {len(lb)} vs {len(ub)})"
        else:
            state = f"far apart ({len(lb)} vs {len(ub)} digits)"
    print(f'{inst[:52]:52s} {str(wall):>7s}  {state}')

print()
print(f"watchdog records whose bounds are already closed (would be proofs): {closed}")
print(f"watchdog records whose bounds are within one digit-length (close)   : {len(narrow)}")
for n in narrow:
    print(f"    {n}")
print()
print("interpretation: 'far apart' with a difference of many digits means the run")
print("was nowhere near a proof when the watchdog cut it, so those zeros are real.")
