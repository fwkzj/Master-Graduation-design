#!/usr/bin/env python3
"""Parallel batch runner for the MSE hybrid-MaxSAT experiments.

One directory per run under runs/<batch>/<config>/<seed>/<instance>/, holding
the exact command, the solver's stdout/stderr, the coordinator's round log and a
status.json summarising the parsed record. A single global experiment.log gets a
line per run, and summary.tsv plus summary.md are written when the batch ends.

The end-to-end budget is enforced here as well as in the runner: each run is
wrapped in `timeout` so a wedged process cannot hold a worker forever. The
runner normally stops itself first, so exit 124 means the wrapper had to fire --
which is a finding, not routine.

Usage
-----
    python3 scripts/run_batch.py --batch stage0 --manifest smoke --budget 60
    python3 scripts/run_batch.py --batch debug50 --manifest debug50_MSE23W
    python3 scripts/run_batch.py --compare                  # Stage 1 vs Stage 2
"""

import argparse
import hashlib
import json
import os
import platform
import re
import shlex
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
from pathlib import Path

ALL_CONFIGS = ["CASH", "SPB", "Hybrid", "HybridNoInference",
               "HybridNatural", "HybridAdaptive", "HybridSelective", "HybridLate"]
ALL_SEEDS = [20260909, 20260910, 20260911]

# Overridden from the command line by --configs/--seeds so a reduced sweep is
# reproducible from its own meta.json rather than from shell history. The two
# primary configurations are the ones the headline comparison rests on; the
# ablation (HybridNoInference) and the standalone local search (SPB) are extra
# context that costs the same wall-clock as the comparison itself.
CONFIGS = ALL_CONFIGS
SEEDS = ALL_SEEDS

# Extra seconds the harness waits beyond the budget before sending SIGTERM.
# The runner's own watchdog fires at budget+3, so a well-behaved run exits 0
# well inside this.
HARNESS_GRACE = 15
KILL_AFTER = 30

# A record's bound is a decimal string, or this sentinel when the configuration
# does not produce that bound at all (SPB derives no lower bound; a run cut off
# by the watchdog has no readable bounds).
NOT_AVAILABLE = "n/a"

# Exit reasons the runner may report, per experiment.md 8.5. `optimum_scip` is
# an optimality proof from CASH's ILP component rather than from its CDCL
# search: a solved instance either way, but one that says the hybrid protocol
# never got the chance to interleave.
EXIT_REASONS = {"optimum", "optimum_scip", "budget", "budget_watchdog",
                "budget_signal", "budget_no_bounds",
                "budget_watchdog_no_bounds", "completed"}

RECORD_RE = re.compile(r'\{"event":"run_complete".*\}')

_print_lock = threading.Lock()


def log(message):
    with _print_lock:
        print(message, flush=True)


class Stopper:
    """Lets a failing debug run stop scheduling new work immediately."""

    def __init__(self):
        self._lock = threading.Lock()
        self._stopped = False
        self.reason = None

    def stop(self, reason):
        with self._lock:
            if not self._stopped:
                self._stopped = True
                self.reason = reason

    @property
    def stopped(self):
        with self._lock:
            return self._stopped


def sha1_of_file(path):
    digest = hashlib.sha1()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_instances(manifest_path):
    instances = []
    for line in manifest_path.read_text().splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            instances.append(line)
    if not instances:
        raise SystemExit(f"manifest is empty: {manifest_path}")
    return instances


def parse_record(stdout_text):
    """Return the last run_complete record, or None if the run wrote none."""
    records = RECORD_RE.findall(stdout_text)
    if not records:
        return None
    try:
        return json.loads(records[-1])
    except json.JSONDecodeError:
        return None


def brief(value, limit=40):
    """Shorten a bound for a log line: a 421807-digit number has no place there."""
    text = str(value)
    if len(text) <= limit:
        return text
    return f"{text[:limit]}...({len(text)} digits)"


def compare_bounds(low, high):
    """Compare two decimal bound strings without converting them to int.

    MSE23W carries instances whose weights are so large that a bound runs to
    hundreds of thousands of digits (abstraction-refinement_wt-polysite-bloat
    reports 421807). CPython refuses int()/str() round-trips beyond 4300 digits
    by default, and raising the limit only moves the ceiling -- arithmetic on
    such numbers is quadratic and pointless here. Both bounds are non-negative
    decimal integers, so comparing digit length first and then lexicographically
    gives the same order in linear time.
    """
    low, high = str(low), str(high)
    low, high = low.lstrip("-"), high.lstrip("-")
    if len(low) != len(high):
        return -1 if len(low) < len(high) else 1
    if low == high:
        return 0
    return -1 if low < high else 1


def bound_is_number(text):
    return isinstance(text, str) and re.fullmatch(r"-?\d+", text) is not None


def validate(record):
    """Reject records that cannot be trusted as measurements.

    These are the Stage 1 gate conditions: an invalid bound, an optimality claim
    with nothing behind it, or a run whose end state is not one the protocol can
    produce. A run that legitimately ends without bounds ("n/a") is not a
    failure on its own -- only a claim that contradicts the values is.
    """
    problems = []
    reason = record.get("exit_reason", "")
    low, high = record.get("final_lb"), record.get("final_ub")
    if bound_is_number(low) and bound_is_number(high) and compare_bounds(low, high) > 0:
        problems.append(f"lower bound {brief(low)} exceeds upper bound {brief(high)}")
    if record.get("cash_proved_optimal"):
        if not bound_is_number(high):
            problems.append("claims optimality without an upper bound")
        elif not bound_is_number(low) or compare_bounds(low, high) != 0:
            problems.append(
                f"claims optimality but bounds differ: "
                f"{brief(low)} vs {brief(high)}")
    if reason.endswith("no_bounds"):
        problems.append("run ended without readable bounds")
    if reason not in EXIT_REASONS:
        problems.append(f"unknown exit_reason {reason!r}")
    # The two proof reasons are the only ones that may accompany an optimality
    # claim; a run that claims optimality while reporting a budget stop is
    # self-contradictory whichever component proved it.
    if record.get("cash_proved_optimal") and reason not in ("optimum", "optimum_scip"):
        problems.append(f"claims optimality but ended with {reason!r}")
    return problems


def run_one(task, args, stopper, meta):
    instance, config, seed = task
    instance_path = Path(args.dataset_root) / instance
    name = Path(instance).stem
    run_dir = Path(args.runs_root) / args.batch / config / str(seed) / name
    run_dir.mkdir(parents=True, exist_ok=True)

    events_path = run_dir / "events.jsonl"
    stdout_path = run_dir / "stdout.log"
    stderr_path = run_dir / "stderr.log"

    cmd = [
        str(Path(args.binary).resolve()),
        "--config", config,
        "--seed", str(seed),
        "--budget", str(args.budget),
        "--cash-window", str(args.cash_window),
        "--spb-window", str(args.spb_window),
        "--strat", str(args.strat),
        "--instance-id", instance,
        str(instance_path),
        str(events_path),
    ]
    # Zero leaves the SCIP bound to the binary, which derives it from the CASH
    # window; a batch that sets it pins the value for all four configurations.
    if args.scip_cpu > 0:
        cmd += ["--scip-cpu", str(args.scip_cpu)]
    wrapped = ["timeout", "--signal=TERM", f"--kill-after={KILL_AFTER}",
               str(args.budget + HARNESS_GRACE)] + cmd
    (run_dir / "cmd.txt").write_text(" ".join(shlex.quote(c) for c in wrapped) + "\n")

    started = time.time()
    with open(stdout_path, "wb") as out, open(stderr_path, "wb") as err:
        completed = subprocess.run(wrapped, stdout=out, stderr=err, check=False)
    wall = time.time() - started

    stdout_text = stdout_path.read_text(errors="replace")
    record = parse_record(stdout_text)

    problems = []
    # A round can complete without SPB producing a feasible solution (`spb_ub`
    # stays -1). Counting both makes the difference between "the handoff never
    # happened" and "the handoff happened but SPB had nothing to contribute"
    # visible in the summary instead of hidden inside the event stream.
    rounds = 0
    productive_rounds = 0
    if events_path.exists():
        for line in events_path.read_text(errors="replace").splitlines():
            if not line.strip():
                continue
            try:
                event = json.loads(line)
            except json.JSONDecodeError:
                problems.append(f"unparsable event: {line[:80]}")
                continue
            # Only handoffs are rounds. The stream also carries progress
            # records (one per scheduling point); counting those would inflate
            # this column for every configuration.
            if event.get("event") != "spb_round":
                continue
            rounds += 1
            if event.get("spb_ub", -1) >= 0:
                productive_rounds += 1

    if record is None:
        problems.append(f"no run_complete record (exit {completed.returncode})")
    else:
        problems.extend(validate(record))
    if completed.returncode not in (0, 124):
        problems.append(f"unexpected exit code {completed.returncode}")

    status = {
        "instance": instance,
        "config": config,
        "seed": seed,
        "exit_code": completed.returncode,
        "wall_seconds": round(wall, 3),
        "harness_timed_out": completed.returncode == 124,
        "rounds": rounds,
        "productive_rounds": productive_rounds,
        "record": record,
        "problems": problems,
        "binary_sha1": meta["binary_sha1"],
        "finished_at": datetime.now(timezone.utc).isoformat(),
    }
    (run_dir / "status.json").write_text(json.dumps(status, indent=2) + "\n")

    verdict = "OK" if not problems else "FAIL"
    log(f"{verdict} {config:18s} seed={seed} {name[:48]:48s} "
        f"exit={completed.returncode} wall={wall:6.1f}s rounds={rounds}")

    with open(Path(args.runs_root) / args.batch / "experiment.log", "a") as handle:
        handle.write(f"{status['finished_at']}\t{verdict}\t{config}\t{seed}\t"
                     f"{instance}\t{completed.returncode}\t{wall:.3f}\t{rounds}\t"
                     f"{'; '.join(problems)}\n")

    if problems and args.mode == "debug":
        stopper.stop(f"{config}/{seed}/{instance}: {'; '.join(problems)}")
    return status


def write_meta(args, meta):
    meta_dir = Path(args.runs_root) / args.batch / "meta"
    meta_dir.mkdir(parents=True, exist_ok=True)
    (meta_dir / "meta.json").write_text(json.dumps(meta, indent=2) + "\n")


def summarise(args, statuses, meta):
    batch_dir = Path(args.runs_root) / args.batch
    lines = ["config\tseed\truns\tok\tfailed\twith_ub\tproved_optimal\t"
             "via_scip\tbudget_stops\tmean_wall\tmean_rounds\trounds_total\t"
             "productive_rounds"]
    for config in CONFIGS:
        for seed in SEEDS:
            group = [s for s in statuses if s["config"] == config and s["seed"] == seed]
            if not group:
                continue
            ok = sum(1 for s in group if not s["problems"])
            # A run "has a UB" when it ended holding a feasible solution, which
            # is what the solution-quality comparison is built on. Runs that
            # ended without one are visible as with_ub < runs rather than being
            # silently averaged in.
            with_ub = sum(1 for s in group if s["record"]
                          and bound_is_number(s["record"].get("final_ub")))
            optimal = sum(1 for s in group
                          if s["record"] and s["record"].get("cash_proved_optimal"))
            # The subset of `proved_optimal` that never exercised the protocol:
            # SCIP answered the instance inside its own time limit, before the
            # first CASH window elapsed, so no round could happen. Reported
            # separately because a large number here means the CASH-vs-Hybrid
            # comparison has nothing to measure on those instances rather than
            # that the two configurations agreed.
            via_scip = sum(1 for s in group
                           if s["record"] and s["record"].get("scip_proved_optimal"))
            budget = sum(1 for s in group
                         if s["record"] and s["record"].get("exit_reason") == "budget")
            mean_wall = sum(s["wall_seconds"] for s in group) / len(group)
            rounds = sum(s["rounds"] for s in group)
            productive = sum(s["productive_rounds"] for s in group)
            lines.append(
                f"{config}\t{seed}\t{len(group)}\t{ok}\t{len(group) - ok}\t"
                f"{with_ub}\t{optimal}\t{via_scip}\t{budget}\t{mean_wall:.1f}\t"
                f"{rounds / len(group):.2f}\t{rounds}\t{productive}")
    (batch_dir / "summary.tsv").write_text("\n".join(lines) + "\n")

    # The hybrid configurations only interleave on instances where CASH's own
    # loop iterates between SAT calls; this makes that visible per batch instead
    # of leaving it to be inferred from the objective values.
    hybrid = [s for s in statuses if s["config"].startswith("Hybrid")]
    if hybrid:
        silent = sum(1 for s in hybrid if s["rounds"] == 0)
        barren = sum(1 for s in hybrid
                     if s["rounds"] > 0 and s["productive_rounds"] == 0)
        lines.append("")
        lines.append(f"# hybrid runs with zero SPB rounds: {silent}/{len(hybrid)}")
        lines.append(f"# hybrid runs whose SPB rounds never returned a feasible "
                     f"solution: {barren}/{len(hybrid)}")
    # How often CASH's ILP component answered the instance outright. On these
    # runs the hybrid protocol is never entered, so they carry no information
    # about the CASH-vs-Hybrid difference and must not be read as agreement.
    cash_like = [s for s in statuses
                 if s["config"] in ("CASH", "Hybrid", "HybridNoInference",
                                    "HybridNatural", "HybridAdaptive")]
    if cash_like:
        via_scip = sum(1 for s in cash_like
                       if s["record"] and s["record"].get("scip_proved_optimal"))
        lines.append("")
        lines.append(f"# CASH-based runs solved by SCIP before any CASH window "
                     f"elapsed: {via_scip}/{len(cash_like)}")
    # Records the watchdog had to write. Reaching the watchdog means the run
    # outlived its budget plus one window, so these are protocol findings
    # rather than routine budget stops and must not be averaged in with them.
    watchdog = [s for s in statuses
                if s["record"]
                and str(s["record"].get("exit_reason", "")).startswith(
                    "budget_watchdog")]
    if watchdog:
        lines.append("")
        lines.append(f"# runs ended by the watchdog: {len(watchdog)}/{len(statuses)}")
    (batch_dir / "summary.md").write_text("# " + "\n# ".join(lines) + "\n")
    log(f"wrote {batch_dir / 'summary.tsv'}")
    return lines


def make_manifests(args):
    """Deterministically sample and enumerate the MSE instance pools."""
    out = Path(args.manifest_dir)
    out.mkdir(parents=True, exist_ok=True)
    for sub in ("MSE23W", "MSE24W"):
        root = Path(args.dataset_root) / sub
        if not root.is_dir():
            continue
        names = sorted(p.name for p in root.glob("*.wcnf"))
        (out / f"full_{sub}.txt").write_text("".join(f"{sub}/{n}\n" for n in names))
        log(f"full_{sub}.txt: {len(names)} instances")

    full = (out / "full_MSE23W.txt")
    if full.exists():
        names = [line.strip() for line in full.read_text().splitlines() if line.strip()]
        # Seeded shuffle so the debug sample is reproducible and independent of
        # directory iteration order.
        import random
        rng = random.Random(20260909)
        sample = sorted(rng.sample(names, min(args.debug_size, len(names))))
        (out / f"debug{args.debug_size}_MSE23W.txt").write_text("".join(f"{n}\n" for n in sample))
        log(f"debug{args.debug_size}_MSE23W.txt: {len(sample)} instances")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--batch", help="batch name, also the runs/ subdirectory")
    parser.add_argument("--manifest", help="manifest name under manifests/")
    parser.add_argument("--binary", default="build/HybridMaxSAT/hybridmaxsat")
    parser.add_argument("--dataset-root", default="/data/dataset/Maxsat/Complete")
    parser.add_argument("--runs-root", default="runs")
    parser.add_argument("--manifest-dir", default="manifests")
    parser.add_argument("--budget", type=int, default=600)
    parser.add_argument("--cash-window", type=int, default=15)
    parser.add_argument("--spb-window", type=int, default=3)
    parser.add_argument("--strat", type=int, default=0,
                        help="S line stratification boundary policy: 0 geometric, 1 mass, 2 harden-aligned")
    parser.add_argument("--scip-cpu", type=float, default=0,
                        help="0 derives CASH's SCIP bound from the CASH window")
    parser.add_argument("--workers", type=int, default=32)
    parser.add_argument("--mode", choices=["debug", "full"], default="debug",
                        help="debug stops the batch at the first failure")
    parser.add_argument("--limit", type=int, default=0,
                        help="run only the first N tasks (smoke testing)")
    parser.add_argument("--make-manifests", action="store_true")
    parser.add_argument("--debug-size", type=int, default=50)
    parser.add_argument("--configs", default=",".join(ALL_CONFIGS),
                        help="comma-separated subset of the four configurations")
    parser.add_argument("--seeds", default=",".join(str(s) for s in ALL_SEEDS),
                        help="comma-separated seeds")
    args = parser.parse_args()

    # main() is the only writer of these, and summarise()/meta read them, so
    # rebinding the module globals here keeps both consistent with --configs.
    global CONFIGS, SEEDS
    CONFIGS = [c.strip() for c in args.configs.split(",") if c.strip()]
    SEEDS = [int(s) for s in args.seeds.split(",") if s.strip()]
    unknown = [c for c in CONFIGS if c not in ALL_CONFIGS]
    if unknown:
        parser.error(f"unknown config(s): {', '.join(unknown)}")
    if not CONFIGS or not SEEDS:
        parser.error("--configs and --seeds must both be non-empty")

    if args.make_manifests:
        make_manifests(args)
        return 0
    if not args.batch or not args.manifest:
        parser.error("--batch and --manifest are required")

    manifest_path = Path(args.manifest_dir) / f"{args.manifest}.txt"
    if not manifest_path.exists():
        raise SystemExit(f"no such manifest: {manifest_path}")
    instances = read_instances(manifest_path)

    binary = Path(args.binary)
    if not binary.is_file():
        raise SystemExit(f"no such binary: {binary}")

    meta = {
        "batch": args.batch,
        "manifest": str(manifest_path),
        "manifest_sha1": sha1_of_file(manifest_path),
        "instance_count": len(instances),
        "configs": CONFIGS,
        "seeds": SEEDS,
        "budget_seconds": args.budget,
        "cash_window_seconds": args.cash_window,
        "strat_policy": args.strat,
        "spb_window_seconds": args.spb_window,
        "scip_cpu_seconds": args.scip_cpu or None,
        "workers": args.workers,
        "mode": args.mode,
        "harness_grace_seconds": HARNESS_GRACE,
        "kill_after_seconds": KILL_AFTER,
        "binary": str(binary.resolve()),
        "binary_sha1": sha1_of_file(binary),
        "host": platform.node(),
        "uname": " ".join(platform.uname()),
        "cpu_count": os.cpu_count(),
        "started_at": datetime.now(timezone.utc).isoformat(),
    }
    try:
        meta["git_commit"] = subprocess.run(
            ["git", "rev-parse", "HEAD"], capture_output=True, text=True,
            check=False).stdout.strip()
    except OSError:
        meta["git_commit"] = None

    tasks = [(inst, config, seed)
             for inst in instances
             for config in CONFIGS
             for seed in SEEDS]
    if args.limit:
        tasks = tasks[:args.limit]
    log(f"batch={args.batch} manifest={manifest_path.name} tasks={len(tasks)} "
        f"workers={args.workers} budget={args.budget}s mode={args.mode}")

    (Path(args.runs_root) / args.batch).mkdir(parents=True, exist_ok=True)
    write_meta(args, meta)

    stopper = Stopper()
    statuses = []
    crashed = []
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = {}
        for task in tasks:
            if stopper.stopped:
                break
            futures[pool.submit(run_one, task, args, stopper, meta)] = task
        for future in futures:
            try:
                statuses.append(future.result())
            except Exception as error:  # a worker crash must not lose the batch
                # A raised post-processing error means this task produced no
                # status.json at all, so it would otherwise vanish: not a run,
                # not a failure, and invisible in the exit code. Record it as a
                # crash so the batch cannot report success while dropping a task.
                task = futures[future]
                crashed.append((task, repr(error)))
                log(f"FAIL worker raised on {task[1]}/{task[2]}/{task[0]}: {error!r}")

    summarise(args, statuses, meta)

    failed = [s for s in statuses if s["problems"]]
    if crashed:
        log(f"crashed workers: {len(crashed)} task(s) produced no status.json")
        for task, error in crashed:
            log(f"  {task[1]}/{task[2]}/{task[0]}: {error}")
    if stopper.stopped:
        # Only meaningful if the stop happened while tasks were still being
        # submitted; once every task is in flight the batch always runs out.
        log(f"stopper fired: {stopper.reason}")
    log(f"done: {len(statuses)} runs, {len(failed)} with problems, "
        f"{len(crashed)} crashed")
    return 1 if (failed or crashed) else 0


if __name__ == "__main__":
    sys.exit(main())
