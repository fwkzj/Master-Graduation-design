"""Did SPB actually improve the upper bound?

Three different claims get conflated under "SPB helped", so they are measured
separately:

  A. round level -- SPB's own bound beat CASH's bound at that moment
     (`spb_ub < cash_ub_before_spb`), and whether CASH accepted it.
  B. run level   -- the hybrid ended on a better UB than CASH-only did on the
     same instance and seed. This is what the experiment is ultimately for.
  C. proof level -- did the handoff ever turn a non-proof into a proof.

A run whose record was written by the watchdog (`scip_seconds == 0`, impossible
on the real path) skipped the SPB-certificate backfill, so its `final_ub` is not
comparable and it is reported separately rather than counted as a loss.
"""
import glob
import json
import re
from collections import Counter, defaultdict

ROOT = "runs/debug50"


def num(x):
    """Bounds are decimal strings; SPB's certificate is a bare JSON number."""
    if isinstance(x, bool) or x is None:
        return None
    if isinstance(x, int):
        return x
    if isinstance(x, str) and re.fullmatch(r"-?\d+", x):
        return int(x)
    return None


# ---- A. round level ------------------------------------------------------
rounds = Counter()
per_cfg = defaultdict(Counter)
improved_instances = defaultdict(set)
all_instances = set()
for cfg in ("Hybrid", "HybridNoInference"):
    for ev_path in glob.glob(f"{ROOT}/{cfg}/*/*/events.jsonl"):
        inst = ev_path.split("/")[-2]
        seed = ev_path.split("/")[-3]
        all_instances.add(inst)
        for line in open(ev_path, errors="replace"):
            if not line.strip():
                continue
            try:
                e = json.loads(line)
            except json.JSONDecodeError:
                continue
            rounds[cfg] += 1
            spb = e.get("spb_ub", -1)
            before = e.get("cash_ub_before_spb", -1)
            verdict = e.get("cash_bound_result")
            per_cfg[cfg][verdict] += 1
            if spb is not None and spb >= 0:
                per_cfg[cfg]["feasible"] += 1
                if before is not None and before >= 0:
                    if spb < before:
                        per_cfg[cfg]["beat_cash"] += 1
                        improved_instances[cfg].add((inst, seed))
                    elif spb == before:
                        per_cfg[cfg]["tied_cash"] += 1
                    else:
                        per_cfg[cfg]["worse_than_cash"] += 1
                else:
                    per_cfg[cfg]["cash_ub_unknown"] += 1

print("=== A. round level (SPB's bound vs CASH's bound in the same round) ===")
for cfg in ("Hybrid", "HybridNoInference"):
    c = per_cfg[cfg]
    n = rounds[cfg]
    print(f"{cfg}: {n} rounds")
    print(f"   SPB returned a feasible solution : {c['feasible']:4d}  "
          f"({100.0 * c['feasible'] / n:.1f}%)")
    print(f"   ... stricter than CASH's bound   : {c['beat_cash']:4d}  "
          f"({100.0 * c['beat_cash'] / n:.1f}%)")
    print(f"   ... equal to CASH's bound        : {c['tied_cash']:4d}")
    print(f"   ... weaker than CASH's bound     : {c['worse_than_cash']:4d}")
    print(f"   CASH accepted / not_improved     : "
          f"{c['accepted']} / {c['not_improved']}")
    print(f"   runs where SPB beat CASH at least once: "
          f"{len(improved_instances[cfg])}")

# ---- B. run level --------------------------------------------------------
print()
print("=== B. run level (hybrid final UB vs CASH-only final UB) ===")
runs = defaultdict(dict)
for path in glob.glob(f"{ROOT}/*/*/*/status.json"):
    d = json.load(open(path))
    r = d.get("record") or {}
    if not r:
        continue
    runs[(d["instance"].split("/")[-1].replace(".wcnf", ""), d["seed"])][d["config"]] = {
        "ub": num(r.get("final_ub")),
        "proved": bool(r.get("cash_proved_optimal")),
        "watchdog": r.get("scip_seconds") in (0, "0") and d["config"] != "SPB",
        "cert": num(r.get("spb_certificate_ub")),
    }

verdicts = Counter()
examples = defaultdict(list)
for (inst, seed), cfgmap in sorted(runs.items()):
    cash, hyb = cfgmap.get("CASH"), cfgmap.get("Hybrid")
    if not cash or not hyb or cash["ub"] is None or hyb["ub"] is None:
        continue
    if cash["proved"] and hyb["proved"]:
        v = "both proved (no room for SPB)"
    elif hyb["ub"] < cash["ub"]:
        v = "HYBRID better"
    elif hyb["ub"] == cash["ub"]:
        v = "same UB"
    else:
        v = "HYBRID worse" + (" [watchdog record]" if hyb["watchdog"] else "")
    verdicts[v] += 1
    if v != "both proved (no room for SPB)":
        examples[v].append((inst, seed, cash["ub"], hyb["ub"], hyb["cert"], hyb["watchdog"]))

for v, n in verdicts.most_common():
    print(f"  {v:40s} {n}")
print()
for v in ("HYBRID better", "HYBRID worse"):
    for inst, seed, cu, hu, cert, wd in examples[v]:
        origin = ("SPB certificate backfill" if cert == hu
                  else "CASH accepted it into its own UB" if cert is not None
                  else "?")
        print(f"  [{v}] {inst[:42]:42s} seed={seed} cash={cu} hybrid={hu} "
              f"spb_cert={cert} wd={wd}  origin={origin}")

print()
print("=== B2. where the hybrid's final UB actually came from ===")
src = Counter()
for (inst, seed), cfgmap in runs.items():
    hyb = cfgmap.get("Hybrid")
    if not hyb or hyb["ub"] is None:
        continue
    if hyb["cert"] is None:
        src["no SPB certificate at all (CASH's own UB)"] += 1
    elif hyb["ub"] == hyb["cert"]:
        src["SPB certificate is the final UB"] += 1
    elif hyb["ub"] < hyb["cert"]:
        src["CASH's accepted UB beats SPB's certificate"] += 1
    else:
        src["SPB certificate is worse than final UB"] += 1
for k, v in src.most_common():
    print(f"  {k:46s} {v}")

print()
print("=== B3. is the hybrid's better UB ever from a round CASH did NOT accept? ===")
strict = 0
for (inst, seed), cfgmap in runs.items():
    hyb, cash = cfgmap.get("Hybrid"), cfgmap.get("CASH")
    if not hyb or not cash or hyb["ub"] is None or cash["ub"] is None:
        continue
    if hyb["ub"] < cash["ub"] and hyb["cert"] == hyb["ub"]:
        strict += 1
        print(f"  {inst[:42]:42s} seed={seed}: hybrid {hyb['ub']} == SPB cert, "
              f"CASH-only {cash['ub']}")
print(f"  total carried by SPB's certificate rather than a CASH-accepted bound: {strict}")

# ---- C. proof level ------------------------------------------------------
print()
print("=== C. did a handoff ever produce a proof CASH-only did not get? ===")
flips = 0
for (inst, seed), cfgmap in runs.items():
    cash, hyb = cfgmap.get("CASH"), cfgmap.get("Hybrid")
    if cash and hyb and not cash["proved"] and hyb["proved"]:
        flips += 1
        print(f"  {inst} seed={seed}: CASH-only no proof, hybrid proved")
print(f"  hybrid-proved-where-CASH-only-did-not: {flips}")
