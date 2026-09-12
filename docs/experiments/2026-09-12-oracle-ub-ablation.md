# 实验记录：上界通道有没有价值（CASHOracleUB 消融）

**日期**：2026-09-12
**批次**：`runs/oracleub`（`runs/oracleub.launch.log`）
**提交**：`5714eaa71a69fc4caf2d2811b5c57c4487b285e3`（工作树干净，`meta.json` 记录 binary sha1 `b0afdf966291a0551e1048580ef183f5f0e6fd7f`）
**并发**：64 workers（符合 agent.md 的 64 上限）
**预算/窗口**：600s，`--cash-window 30 --spb-window 10`，seed 20260909
**规模**：550 次运行（275 实例 × CASH / CASHOracleUB），实际用时 3654s（约 61 分钟）

## 1. 这个实验要回答什么

此前所有调度实验（P0–P7 分层、A1 阈值目标、A2 相位注入、A3 选择性交接、adaptive 退避、gate / safe / late）都在混淆两个变量：

- 局部搜索**花掉的时间**（长跑实例中位 15 轮 × 10s ≈ 147s，占 600s 预算的 24.5%）；
- 局部搜索带来的**上界改善**本身的收益。

本实验把二者彻底分开：`CASHOracleUB` 与 `CASH` 基线**逐字相同**，唯一区别是在 CASH 的第一个调度点（实测 t≈0.05s，早于任何核心抽取）注入一条"外部已核验上界"。该上界取自历史上所有 SPB 轮次与该实例有关的**最小值**（`manifests/oracle_ub_full23w.tsv`，由 `spb_ub` / `spb_certificate_ub` 汇总而来，不含 CASH 自己找到的任何界）。注入配置**完全不运行局部搜索**，因此不占用任何预算。

也就是说，这个配置给出的上界通道价值是**上界**：它是"SPB 的最强可能成果，且免费送达"。如果连它都不能提升证明数量，那么真实运行 SPB 的混合配置只可能更差。

## 2. 实现

- `HybridMaxSAT/src/hybrid_main.cpp`：新增 `Config::CASHOracleUB`。`CashOnlyCallback` 增加一个可选 oracle 上界；`should_run()` 只在第一次调度点返回 true，`find_upper_bound()` 返回表里的界后立即置为惰性。其余（deadline 语义、SCIP 限额、watchdog grace、事件文件形状）与 `CASH` 完全一致。注入成功时写一条 `{"event":"oracle_ub",...}` 记录，便于核对。
- `scripts/run_batch.py`：`ALL_CONFIGS` 加入 `CASHOracleUB`（按字节改写，保留该文件原本的 CRLF 行尾）。
- `manifests/oracle_ub_full23w.tsv`：实例→上界表；`manifests/oracleub_MSE23W.txt`：对应的实例清单。

## 3. 结果（配对比较，两组各 275 次运行，同一提交、同一并发）

| 指标 | CASH | CASHOracleUB |
| --- | --- | --- |
| `proved_optimal` | **132** | **127** |
| 其中由 SCIP 直接证明 | 5 | 5 |
| `with_ub` | 259 | 254 |
| mean_wall | 354.6s | 355.8s |
| 有问题的运行 | 5 | 5 |

配对拆分：

| 配对结果 | 实例数 |
| --- | --- |
| 两者都证明 | 127 |
| **只有 CASH 证明** | **5** |
| **只有 oracle 证明** | **0** |
| 两者都没证明 | 138 |

被单方面证明的 5 个实例（全部由 CASH 拿下，oracle 全部失败）：

| 实例 | CASH | CASHOracleUB |
| --- | --- | --- |
| `frb_wt-frb25-13-5.wcnf` | lb=300 ub=300（证明） | lb=291 ub=300 |
| `frb_wt-frb30-15-1.wcnf` | lb=420 ub=420（证明） | lb=410 ub=420 |
| `frb_wt-frb30-15-4.wcnf` | lb=420 ub=420（证明） | lb=401 ub=420 |
| `frb_wt-frb35-17-5.wcnf` | lb=560 ub=560（证明） | lb=534 ub=560 |
| `spot5_wt-5.wcsp.log.wcnf` | lb=261 ub=261（证明） | lb=259 ub=261 |

注意这 5 例里 oracle 运行**手里握着的上界就是最优值**（300/420/420/560/261），但它的下界没能在预算内跟上；CASH 反而做到了。

## 4. 注入确实生效，而且效果很"强"

- 275 个实例中 **270 个真的注入了**（其余 5 个是无效清单项，见 §7）。
- 这 270 个里，**270/270 的注入界都严格优于 CASH 自己的起始上界**（例如 `frb_wt-frb30-15-1`：8073350 → 420，t=0.05s）。
- **95 个实例**上 oracle 运行最终持有的上界严格优于 CASH——上界通道按设计工作。

但即便如此，证明数量没有增加一个。

## 5. 机制观察：差异不在"硬化"

`frb_wt-frb30-15-1` 的两条轨迹（同一 seed）：

| t | CASH | CASHOracleUB |
| --- | --- | --- |
| 0.05 | lb=0 ub=8073350 harden=0 | lb=0 ub=8073350 harden=0 → **注入 420** |
| 11.0 | lb=401 ub=426 harden=17900 | lb=401 ub=420 harden=17900 |
| 66 | lb=403 ub=422 harden=17900 | lb=403 ub=420 harden=17900 |
| 446 | lb=410 ub=422 harden=17900 | lb=410 ub=420 harden=17900 |

两点结论：

1. **硬化次数完全相同**（17900），而且是在 t≈11s 由 CASH 自己的搜索一并触发的，与注入无关。也就是说"UB 改善 → 触发新硬化"这条链条在这里并没有被注入改变。这与 `2026-09-11-hardening-check.md` 的结论一致：硬化几乎全部发生在第一次调度点之前，之后再不动。
2. **下界轨迹在前 450s 逐点相同**，差异只出现在最后约 150s 的收尾阶段——即两个配置的差别不是"搜索被带偏"，而是"最后一步谁先落地"。

## 6. 关键方法学发现：噪声底是 ±4 个证明

把本批次的 CASH 与 `full23w_fix`（提交 `5b83ae8`，128 workers）的 CASH 在同一批 270 个实例上对比：

| 对比项 | 结果 |
| --- | --- |
| 证明/未证明判定一致 | 266/270 |
| 最终 `(lb, ub)` 完全相同 | 182/270 |
| 判定翻转的实例 | 4 个（`abstraction-refinement_wt-polysite-xalan`、`correlation-clustering_wt-Rounded_CorrelationClusteri`、`frb_wt-frb35-17-5`、`tcp_wt-tcp_students_98_it_14`） |

即：**同一个 CASH 基线、同 seed、同预算，在不同批次间会有约 4 个实例的证明数波动**（环境差异带来的扰动，本批次 64 并发 vs 旧批次 128 并发）。所以 132 vs 127 这个 5 的差距**落在噪声带内**，不能断言"注入有害"，但可以确定：**注入没有任何正贡献**（0 胜 5 负）。

副带结论：今后任何单批次"证明数提升"小于约 4 的结论都不可信，需要多 seed 或多批次复现。

## 7. 无效清单项（10 个 failed 运行的真因）

`manifests/oracleub_MSE23W.txt` 有 275 行，其中 5 行是从历史事件流里混进来的无效 id（`tests/instances/weight_gcd.wcnf`、`tests/instances/fixed_cost.wcnf`、`build/SPBMAXSAT2-master/.../standard_weigh...` 等），它们既没有数据集文件也没有 `run_complete` 记录。两个配置各 5 个，完全对称，不影响比较。**未修改该清单**：批次 `meta.json` 已经记录了它的 sha1，改动会让批次无法复现；需要重跑时另建一份干净的清单。有效实例数是 **270**。

## 8. 与"SPB 真跑"的对照

同一批 270 个实例，来自 `full23w_fix`：

| 配置 | 证明数 |
| --- | --- |
| CASH | 128 |
| **Hybrid（SPB 真实运行，占预算约 25%）** | **120** |
| CASHOracleUB（免费拿到 SPB 史上最好上界） | 127 |

配对拆分（CASH vs Hybrid）：两者都证明 119，**只有 CASH 证明 9**，只有 Hybrid 证明 1，都没证明 141。

## 9. 结论

1. **上界通道对"证明数量"没有正价值。** 把 SPB 历史上找到的**最好**上界在 t=0 免费送给 CASH，证明数没有增加（0 胜 5 负，且这 5 个落在噪声带内）。既然免费的最强上界都不涨，花钱运行 SPB 取得更弱的成果只会更差。
2. **混合算法相对 CASH 的全部亏空来自时间成本，不是来自上界的副作用。** 真实 Hybrid 在这 270 个实例上是 120 vs 128；而把 SPB 的时间成本置零、只保留其上界，差距就缩到 127 vs 132（≈噪声）。
3. **"初段调用 SPB 以提前硬化高权子句、末段调用以加速收敛"这条思路可以关闭了。** 初段的硬化链条实测不发生（注入前后 harden_count 完全一致，且硬化在 t≈11s 由 CASH 自己触发）；末段的收尾阶段本来就只有 1.9% 的接受率、0.2% 的平均改善，还要占用最宝贵的收尾时间。
4. **下一步只能在下界侧做。** MSE 2026 的无权重精确赛道给出的方向是：增量下界（跨决策层复用局部核心，MaxCDCL2026）、按需增量基数约束编码、further lookahead 式的核心合并、以及 portfolio（UWrMaxSAT2026 用 OLL + SCIP 双线程互换界，第一名 424 vs 本基线 414）。详见 `docs/literature/mse2026-unweighted-survey.md`。

## 10. 复现

```bash
cd /home/fwkzj/HybridAlgorithm
git checkout 5714eaa
ORACLE_UB_MAP=manifests/oracle_ub_full23w.tsv \
  python3 scripts/run_batch.py --batch oracleub --manifest oracleub_MSE23W \
  --configs CASH,CASHOracleUB --seeds 20260909 \
  --budget 600 --cash-window 30 --spb-window 10 --workers 64 --mode full
```
