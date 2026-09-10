"""On the instances the hybrid loses, is its answer worse than either component?

If Hybrid's UB is worse than both CASH-only's and standalone SPB's on the same
instance, then the protocol is not trading one strength for another -- it is
destroying both, and the loss cannot be blamed on SPB being weak.

causal_Link is the motivating case: CASH-only proves ~197k, standalone SPB
reaches ~33.6M, and the hybrid reports ~195.9M, a thousand times worse than the
optimum and six times worse than SPB alone.
"""
import glob
import json
import re


def num(x):
    if isinstance(x, bool) or x is None:
        return None
    if isinstance(x, int):
        return x
    if isinstance(x, str) and re.fullmatch(r"-?\d+", x):
        return int(x)
    return None


def load(config):
    out = {}
    for path in glob.glob(f"runs/debug50/{config}/*/*/status.json"):
        d = json.load(open(path))
        inst = d["instance"].split("/")[-1].replace(".wcnf", "")
        r = d.get("record") or {}
        out[(inst, d["seed"])] = {
            "proved": bool(r.get("cash_proved_optimal")),
            "ub": num(r.get("final_ub")),
            "lb": num(r.get("final_lb")),
            "reason": r.get("exit_reason", "?"),
        }
    return out


cash, hyb, spb = load("CASH"), load("Hybrid"), load("SPB")
keys = sorted(set(cash) & set(hyb))

lost = [k for k in keys if cash[k]["proved"] and not hyb[k]["proved"]]

print("=== on the 27 instances CASH proved and Hybrid did not ===")
print("best UB each configuration reached (lower is better)")
print()
print(f'{"instance":42s} {"seed":>8s} {"CASH-only":>14s} {"SPB-alone":>14s} '
      f'{"Hybrid":>14s}  verdict')
worse_than_both = 0
for k in sorted(lost):
    c, h, s = cash[k]["ub"], hyb[k]["ub"], spb.get(k, {}).get("ub")
    verdict = ""
    if c is not None and h is not None:
        if s is not None and h > c and h > s:
            verdict = "worse than BOTH"
            worse_than_both += 1
        elif h > c:
            verdict = "worse than CASH-only"
    print(f'{k[0][:42]:42s} {k[1]:>8} {str(c):>14s} {str(s):>14s} {str(h):>14s}  {verdict}')

print()
print(f"hybrid worse than both components: {worse_than_both}/{len(lost)}")

print("\n=== causal_Link side by side ===")
for k in sorted(keys):
    if "causal_Link" not in k[0]:
        continue
    print(f'  seed={k[1]}  CASH-only={cash[k]["ub"]} ({cash[k]["reason"]})   '
          f'SPB-alone={spb.get(k, {}).get("ub")} ({spb.get(k, {}).get("reason")})   '
          f'Hybrid={hyb[k]["ub"]} ({hyb[k]["reason"]})')
