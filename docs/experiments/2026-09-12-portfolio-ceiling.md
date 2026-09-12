# 实验记录：既有变体的 portfolio 上限（零成本复算）

**日期**：2026-09-12
**数据**：`runs/` 下已存在的全部批次（不新增任何运行）
**目的**：把"多策略组合"这条路线的收益上限先算出来，避免再花机时去试。

## 方法

若两个策略各自作为独立进程跑满 600s（这正是双线程 portfolio 给每个策略的资源），则 portfolio 的证明数就是两个已证明实例集合的**并集**大小。因此可以直接用历史数据算出上限，无需重跑。

## 1. 同一批次内的并集（CASH + 混合变体）

| 批次 | 配置数 | 最好单配置 | 并集 | 增益 |
| --- | --- | --- | --- | --- |
| `full23w_fix` | 2 | 395 | 396 | +1 |
| `full23w_selective` | 2 | 395 | 395 | 0 |
| `full2cfg` | 2 | 398 | 398 | 0 |
| `full_gate_phase` | 3 | 398 | 399 | +1 |
| `full_safe` | 2 | 399 | 399 | 0 |
| `full_a1_cash_late` | 2 | 394 | 395 | +1 |
| `debug50*` / `sample_multi`（50 实例） | 2–5 | 36 | 36 | 0 |

所有混合变体与 CASH 的并集增益都是 **0 或 +1**：这些调度变体彼此**不互补**。

## 2. 五种分层策略（`--strat` p0–p4）跨批次并集

五个批次都是纯 CASH、同一 558 实例集、只换 `--strat`：

| 策略 | p0 | p1 | p2 | p3 | p4 |
| --- | --- | --- | --- | --- | --- |
| 证明数 | 379 | 373 | 374 | 377 | 373 |

两两并集增益 +1 ~ +5；五个全并 = **386**（相对最好单策略 +7）。

但注意：把这五个策略与普通 CASH 参照组（`full23w_fix/CASH` 395、`full_safe/CASH` 399）合并，并集是 **399 / 403**，相对最好单配置只有 **+4**——正好落在噪声带内（见 `2026-09-12-oracle-ub-ablation.md` §6：同一 CASH 基线跨批次波动约 ±4）。

**而且五个分层策略单独都打不过普通 CASH**（373–379 vs 395–399），它们的并集 386 也够不到普通 CASH。

## 3. 全部历史的并集

取 `runs/` 下所有证明数 ≥50 的配置（29 个，含不同提交、不同并发、不同 seed）：

| 项 | 值 |
| --- | --- |
| 最好单配置 | `full_safe/CASH` = **399** |
| 全部 29 个配置的并集 | **405** |
| 增益 | **+6** |

而"同一个配置跑多次"本身就产生 394 / 395 / 398 / 399 这样的散布。因此 +6 里可归因于**策略多样性**的部分约为 0–2，其余是环境噪声。

## 4. 结论

1. **在 MSE23W + 当前 CASH 核上，"把已有变体组合起来"这条路线的上限就是噪声。** 所有变体改的都是"什么时候、往哪里花时间"，没有引入新的推理机制；同一搜索算法的不同调度之间本来就不互补。
2. MSE 2026 里 portfolio 之所以能加 15–20 题（`MaxCDCL2026 401 → S2O3 415`、`WMaxCDCL2026 391 → S6O3 417`、`CoreForge 357 → ILP 372`），靠的是组合**不同算法**（Open-WBO + BnB、OLL + ILP），而不是同一算法的不同参数。
3. 因此要让融合算法真正超过 CASH，需要引入一个**互补的推理组件**，而不是继续调 SPB 的调度。官方族级数据显示互补点在**下界侧**：`minimizing-pentagons` CASH 0/15 而 MaxCDCL 系 14–15；`asp-optimization` CASH 4/20 而 MaxHS 20/20、EvalMaxSAT 19/20；`single-machine-scheduling` CASH 6/15 而 MaxCDCL-S2O3 11/15。这三族合计就是约 35 题的空白，远超噪声带。
4. 参考实现路径：`UWrMaxSat-MaxCDCL`（MSE 2026 第 2 名，423 题）做的正是"UWrMaxSat + 顺序兜底 MaxCDCL，剩余 LB–UB 间隙仍大时切换"，且其描述明确说该桥接代码由 Codex 协助开发。

## 5. 复现

```bash
cd /home/fwkzj/HybridAlgorithm
# 并集统计只读 runs/ 下的 status.json，不产生新的运行
python3 - <<'PY'
import glob, json, os
def proved(batch, config):
    out = set()
    for p in glob.glob(os.path.join("runs", batch, config, "*", "*", "status.json")):
        s = json.load(open(p))
        if (s.get("record") or {}).get("cash_proved_optimal"):
            out.add(s["instance"])
    return out
print(len(proved("full_safe", "CASH")))
PY
```
