"""Why did one task produce no status.json while its stdout holds a record?

The batch ran 1115 of 1116 tasks cleanly. For the missing one the record is
present in stdout.log, so the failure is in the runner's post-processing, not in
the solver. ThreadPoolExecutor swallows exceptions in futures nobody collects,
so the likely path is that parse_record()/validate() raised on this record and
the task was dropped without a log line.

The record on this instance carries an absurdly large integer bound, which is
the obvious suspect: CPython limits int()<->str conversions to 4300 digits by
default, and both parse_record and validate convert bounds.
"""
import json
import sys

sys.path.insert(0, "scripts")
from run_batch import parse_record, validate  # noqa: E402

PATH = ("runs/full2cfg/CASH/20260909/"
        "abstraction-refinement_wt-polysite-bloat/stdout.log")

text = open(PATH, errors="replace").read()
print(f"stdout bytes: {len(text)}")
print(f"sys.get_int_max_str_digits(): {sys.get_int_max_str_digits()}")

try:
    record = parse_record(text)
except Exception as exc:
    print(f"parse_record RAISED: {type(exc).__name__}: {exc}")
    raise SystemExit(1)

print(f"parse_record -> {'None' if record is None else 'record ok'}")
if record is None:
    raise SystemExit(1)

for key, value in record.items():
    s = str(value)
    shown = s[:70]
    ellipsis = "..." if len(s) > 70 else ""
    print(f"  {key} = {shown}{ellipsis}   (chars={len(s)})")

try:
    print(f"validate -> {validate(record)}")
except Exception as exc:
    print(f"validate RAISED: {type(exc).__name__}: {exc}")

# Does merely round-tripping the record through json survive?
try:
    json.dumps(record)
    print("json.dumps(record) -> ok")
except Exception as exc:
    print(f"json.dumps(record) RAISED: {type(exc).__name__}: {exc}")
