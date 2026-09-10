#!/usr/bin/env python3
"""Per-instance comparison of the configurations in a batch.

Usage on S122:
    python3 scripts/iter_report.py runs/<batch> CASH Hybrid HybridNatural

Prints one row per instance and seed, then the paired counts that the headline
comparison rests on: for each other configuration, how often CASH proved the
optimum and that configuration did not, and vice versa.
"""

import json
import sys
from collections import defaultdict
from pathlib import Path


def load(batch):
    records = {}
    for status in batch.glob("*/*/*/status.json"):
        data = json.loads(status.read_text())
        record = data.get("record")
        if not record:
            continue
        config = status.parts[-4]
        instance = record["instance"]
        # Index by the seed inside the record, not the directory name, so the
        # key used for lookups is the same type the pairing loop sees.
        seed = record["seed"]
        events = status.parent / "events.jsonl"
        rounds = productive = 0
        if events.exists():
            for line in events.read_text(errors="replace").splitlines():
                try:
                    event = json.loads(line)
                except ValueError:
                    continue
                if event.get("event") != "spb_round":
                    continue
                rounds += 1
                if (event.get("spb_ub") or -1) >= 0:
                    productive += 1
        records[(config, instance, seed)] = {
            "config": config, "seed": seed, "instance": instance,
            "proved": bool(record.get("cash_proved_optimal")),
            "via_scip": bool(record.get("scip_proved_optimal")),
            "lb": record.get("final_lb"), "ub": record.get("final_ub"),
            "wall": record.get("wall_seconds"),
            "reason": record.get("exit_reason"),
            "rounds": rounds, "productive": productive,
        }
    return records


def main():
    batch = Path(sys.argv[1])
    configs = sys.argv[2:] or ["CASH", "Hybrid", "HybridNatural"]
    records = load(batch)
    if not records:
        raise SystemExit(f"no records under {batch}")

    keys = sorted({(r["instance"], r["seed"]) for r in records.values()})
    names = sorted({r["config"] for r in records.values()})

    header = f"{'instance':<58} {'seed':<9}" + "".join(
        f"{c[:9]:>11}" for c in names)
    print(header)
    print("-" * len(header))
    for instance, seed in keys:
        row = f"{instance[:58]:<58} {seed:<9}"
        for config in names:
            r = records.get((config, instance, seed))
            if r is None:
                row += f"{'-':>11}"
            elif r["proved"]:
                row += f"{'PROVED':>11}"
            else:
                row += f"{(r['ub'] or 'n/a')[:11]:>11}"
        print(row)

    print()
    print(f"{'config':<18}{'runs':>6}{'proved':>8}{'via_scip':>10}"
          f"{'mean_wall':>11}{'rounds':>9}{'productive':>12}")
    for config in names:
        group = [r for r in records.values() if r["config"] == config]
        proved = sum(1 for r in group if r["proved"])
        scip = sum(1 for r in group if r["via_scip"])
        wall = sum(r["wall"] or 0 for r in group) / len(group)
        rounds = sum(r["rounds"] for r in group)
        productive = sum(r["productive"] for r in group)
        print(f"{config:<18}{len(group):>6}{proved:>8}{scip:>10}"
              f"{wall:>11.1f}{rounds:>9}{productive:>12}")

    print()
    print("paired against CASH:")
    for config in names:
        if config == "CASH":
            continue
        both = cash_only = other_only = neither = 0
        for instance, seed in keys:
            c = records.get(("CASH", instance, seed))
            o = records.get((config, instance, seed))
            if not c or not o:
                continue
            if c["proved"] and o["proved"]:
                both += 1
            elif c["proved"]:
                cash_only += 1
            elif o["proved"]:
                other_only += 1
            else:
                neither += 1
        print(f"  {config:<18} both={both:<4} CASH only={cash_only:<4} "
              f"{config} only={other_only:<4} neither={neither}")

    print()
    print("round counts by configuration (a round is where SPB ran at all):")
    for config in names:
        group = [r for r in records.values() if r["config"] == config]
        silent = sum(1 for r in group if r["rounds"] == 0)
        if any(r["rounds"] for r in group):
            print(f"  {config:<18} zero-round runs={silent}/{len(group)}")


if __name__ == "__main__":
    main()

