"""Verify the strongest claim in the comparison: no hybrid run that ever handed
off to SPB proved optimal (0 of 265), while CASH proved 105 of the same 265.

A claim this clean is more likely to be a recording artifact than a fact, so
check the ways a proof could go unrecorded:

  * the record is a defect-six watchdog record (scip_seconds == 0), which is
    built from a default-constructed summary and always reports
    cash_proved_optimal = false -- if those runs were about to prove, the zero
    would be manufactured;
  * exit_reason could be something other than "budget", meaning the run ended
    for a reason that would not have let a proof through anyway.

Also confirm the 0 is not merely the definition: `rounds` counts coordinator
round events, so a run that proves inside the first window logs zero rounds and
is excluded from the 265 by construction. That exclusion is legitimate -- those
runs never handed off -- but it should be stated rather than assumed.
"""
import glob
import json
from collections import Counter

REASONS = Counter()
WATCHDOG = 0
handoff = []
no_handoff = []

for path in glob.glob("runs/full2cfg/Hybrid/*/*/status.json"):
    d = json.load(open(path))
    r = d.get("record") or {}
    entry = (d["instance"].split("/")[-1].replace(".wcnf", ""), r, d.get("rounds", 0))
    (handoff if d.get("rounds", 0) > 0 else no_handoff).append(entry)

print(f"hybrid runs with >=1 round (handoff) : {len(handoff)}")
print(f"hybrid runs with  0 rounds (no handoff): {len(no_handoff)}")
print()

for label, group in (("handoff", handoff), ("no handoff", no_handoff)):
    reasons = Counter(e[1].get("exit_reason", "?") for e in group)
    proved = sum(1 for e in group if e[1].get("cash_proved_optimal"))
    wd = sum(1 for e in group
             if e[1].get("scip_seconds") == 0 and e[1].get("exit_reason") == "budget")
    print(f"--- {label}: {len(group)} runs, {proved} proved")
    print(f'    exit_reason: ' + "  ".join(f"{k}={v}" for k, v in sorted(reasons.items())))
    print(f"    watchdog-written records: {wd}")
    empty = sum(1 for e in group if not e[1])
    if empty:
        print(f"    records missing entirely: {empty}")

print()
print("=== the handoff runs, by rounds (are any close to proving?) ===")
buckets = Counter()
for inst, r, rounds in handoff:
    if rounds >= 30:
        buckets["30+ rounds"] += 1
    elif rounds >= 10:
        buckets["10-29 rounds"] += 1
    elif rounds >= 2:
        buckets["2-9 rounds"] += 1
    else:
        buckets["1 round"] += 1
for k in ("1 round", "2-9 rounds", "10-29 rounds", "30+ rounds"):
    print(f"  {k:14s} {buckets[k]:>4d}")

print()
print("=== sanity: do any handoff runs claim a proof under any exit_reason? ===")
any_proof = [e for e in handoff if e[1].get("cash_proved_optimal")]
print(f"  {len(any_proof)}")
for inst, r, rounds in any_proof[:10]:
    print(f"    {inst} rounds={rounds} reason={r.get('exit_reason')}")
