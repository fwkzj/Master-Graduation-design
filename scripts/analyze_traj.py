"""Trajectory of CASH's own UB across the hybrid's windows.

The question this answers: on instances where the hybrid ends far worse than
CASH-only, did CASH's upper bound ever move at all while it was being scheduled
in 15-second windows? If it sits at its initial value for every window, the
windows are not "less CASH time" -- they are CASH making no progress.
"""
import json
import sys
from pathlib import Path

root = Path(sys.argv[1] if len(sys.argv) > 1 else "/home/fwkzj/HybridAlgorithm/runs/debug50")
TARGETS = sys.argv[2:] or ["causal-discovery_wt-causal_Link_10_1000",
                           "abstraction-refinement_wt-downcast-pmd",
                           "causal-discovery_wt-causal_alarm_9_1000"]

for target in TARGETS:
    print("=" * 78)
    print(target)
    for cfg in ("CASH", "Hybrid", "HybridNoInference", "SPB"):
        for seed_dir in sorted((root / cfg).glob("*")):
            run_dir = seed_dir / target
            if not run_dir.is_dir():
                continue
            st = run_dir / "status.json"
            rec = json.loads(st.read_text())["record"] if st.exists() else None
            status = json.loads(st.read_text()) if st.exists() else {}
            ev = run_dir / "events.jsonl"
            ubs = []
            if ev.exists():
                for line in ev.read_text(errors="replace").splitlines():
                    if line.strip():
                        try:
                            ubs.append(json.loads(line).get("cash_ub_before_spb"))
                        except json.JSONDecodeError:
                            pass
            uniq = len(set(ubs)) if ubs else 0
            rec = rec or {}
            print(f"  {cfg:18s} {seed_dir.name}  wall={status.get('wall_seconds', 0):6.1f}s "
                  f"rounds={len(ubs):>3d} distinct_cash_ub={uniq:>3d} "
                  f"final_ub={rec.get('final_ub', '?'):>12s} "
                  f"polls={rec.get('cash_deadline_polls', '?')}")
            if ubs:
                head = ubs[:4]
                tail = ubs[-3:] if len(ubs) > 7 else ubs[4:]
                print(f"      cash_ub trajectory: {head} ... {tail}")
