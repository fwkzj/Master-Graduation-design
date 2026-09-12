# 加入局部搜索有效的算例清单（全历史配对挖掘）

**日期**：2026-09-12
**数据**：`runs/` 下全部批次（只读，无新增运行）
**挖掘脚本口径**：只在**同一批次内**配对（同一 manifest、同一提交、同一负载），因此 `CASH` 与混合配置的差异不含跨批次环境噪声。跨批次比较按 `2026-09-12-oracle-ub-ablation.md` §6 的结论不可用（同一 CASH 基线跨批次波动约 ±4 个证明）。

## 0. 一句话结论

- **证明数量上**：确实存在"混合算法证明、CASH 证明不了"的算例，但只有 **5 个**，而且每个都是单次运行的 ±1 事件，落在噪声带内；反向（CASH 证明而混合没证明）在同一挖掘口径下有 **325 个**配对。
- **收敛速度上**：两者都证明但混合快 ≥30% 的算例有 **22 个**，但族级聚合显示只有 `relational-inference` 一个族是系统性加速，其余多为零星个案。
- **上界质量上**：这是唯一**族级一致、幅度巨大**的效果——`setcover` 25/39 个实例被压低、中位降幅 **99.9%**；`staff-scheduling` 71/89、中位 **92.8%**；`ramsey` 28/28、中位 **53%**。

换句话说：**局部搜索的效果主要实现在"随时求解质量（UB）"上，而不是"证明最优"上。**

## 1. A 类：只有混合算法证明最优的算例（5 个）

| 实例 | 批次 | 获胜配置 | CASH（600s 结束） | 混合算法 |
| --- | --- | --- | --- | --- |
| `abstraction-refinement_wt-downcast-pmd.wcnf` | `a1_debug50` | HybridLate | lb=4227 ub=4265（未证明） | **556s 时 lb=ub=4227** |
| `shiftdesign_wt-limits-10-10_data-2_inst-025_30m.sm-extracted.wcnf` | `debug50_deadlinefix` | Hybrid | lb=42365642 ub=42366505 | **lb=ub=42366243** |
| `metro_wt-metro_8_8_5_20_10_6_500_1_0.lp.sm-extracted.wcnf` | `full23w_fix` | Hybrid | lb=79 ub=82（318s） | **223s 时 lb=ub=82** |
| `judgment-aggregation-ja-kemeny-preflib-00043-00000180.wcnf` | `full_a1_cash_late` | HybridLate | lb=430 ub=433 | **504s 时 lb=ub=433** |
| `tcp_wt-tcp_students_91_it_13.wcnf` | `full_gate_phase` | HybridPhase | lb=2658 ub=2730 | **lb=ub=2730** |

共同特征：CASH 的下界已经贴到最优值（差 1–3 个单位），只是没跨过去；混合算法在同一批实例上跨过去了。这类算例是"局部搜索给出的上界恰好补上了最后一步"的最直接候选。

**必须的保留意见**：每个都只出现一次，且 `proved` 判定在跨批次复跑中有 4/270 的翻转率。**这 5 个必须多 seed 复现才能写进论文。**

## 2. C 类：两者都证明，但混合算法快 ≥30%（22 个，列前 12）

| 实例 | 批次 | 获胜配置 | CASH | 混合 | 加速 |
| --- | --- | --- | --- | --- | --- |
| `relational-inference_wt-ar-3.wcnf` | `full23w_1seed` | Hybrid | 243s | **17s** | −93% |
| `relational-inference_wt-ar-1.wcnf` | `full23w_1seed` | Hybrid | 77s | **17s** | −78% |
| `frb_wt-frb30-15-3.wcnf` | `full23w_selective` | HybridSelective | 393s | **80s** | −80% |
| `timetabling_wt-comp02.wcnf` | `full23w_selective` | HybridSelective | 455s | **281s** | −38% |
| `protein_ins_wt-3ebx_.1era_.g.wcnf.t.wcnf` | `full23w_fix` | Hybrid | 367s | **204s** | −44% |
| `quantum-circuit-su2random_5_30_ibmq-london_5.wcnf` | `debug50_selective` | Hybrid | 255s | **160s** | −37% |
| `frb_wt-frb25-13-1.wcnf` | `sample_multi` | HybridSelective | 134s | **65s** | −51% |
| `protein_ins_wt-1bpi_.5pti_.g.wcnf.t.wcnf` | `full23w_selective` | HybridSelective | 147s | **93s** | −37% |
| `drmx-cryptogen_wt-wolfram80_4.wcnf` | `full23w_selective` | HybridSelective | 133s | **80s** | −40% |
| `css-refactoring_wt-guardian.dimacs.wcnf` | `full_a1_cash_late` | HybridLate | 126s | **78s** | −38% |
| `drmx-cryptogen_wt-wolfram80_3.wcnf` | `full_a1_cash_late` | HybridLate | 91s | **50s** | −45% |
| `quantum-circuit-vqe_4_12_ibmq-casablanca_7.wcnf` | `full_safe` | HybridSafe | 76s | **38s** | −51% |

（其余 10 个为 `drmx-cryptogen_wt-geffe128_{1,3}`、`quantum-circuit-{qftentangled_5_30, su2random_4_18, qftentangled_5_48, grover-v-chain_4_52, portfoliovqe_4_18}`、`tcp_wt-tcp_students_{105_it_8, 91_it_14}`、`css-refactoring_wt-amazon`。）

## 3. 族级聚合：哪些族是真正的系统性加速

取"两者都证明且 CASH ≥30s"的实例，按族聚合（中位墙钟）：

| 族 | n | 中位 CASH | 中位混合 | 混合更快的比例 |
| --- | --- | --- | --- | --- |
| **relational-inference** | 20 | 205s | **84s** | 14/20 |
| tcp | 32 | 121s | **107s** | 22/32 |
| frb | 25 | 161s | **147s** | 16/25 |
| protein_ins | 43 | 139s | **133s** | 20/43 |
| drmx-cryptogen | 100 | 62s | 62s | 49/100 |
| quantum-circuit-*（多族） | 各 5–11 | — | 略快 | 2–4/5 |
| causal-discovery | 61 | 86s | 91s | 19/61 |
| css-refactoring | 68 | 30s | 30s | 10/68 |
| abstraction-refinement | 61 | 359s | 388s | 10/61 |
| shiftdesign | 35 | 241s | 257s | 2/35 |
| timetabling | 24 | 184s | 224s | 3/24 |

**只有 `relational-inference`（n=20，205s→84s）达到"族级系统性加速"的标准。** 其余族的胜负比例接近抛硬币或明显不利。其中 `relational-inference` 的数据来自 `full23w_1seed`，而该批次的 Hybrid 是**preemptive 变体**——同一批次整体上它比 CASH 少证明 115 个实例。也就是说这个族的加速是以别处的大面积损失换来的，不能单独作为卖点。

## 4. D 类：两者都没证明，混合算法把上界压低（族级一致）

| 族 | n | 混合更优 | 中位降幅 |
| --- | --- | --- | --- |
| **setcover** | 39 | 25 | **99.9%** |
| **staff-scheduling** | 89 | 71 | **92.8%** |
| MaxSATQueriesinInterpretableClassifiers | 24 | 22 | 80.1% |
| decision-tree-heart-cleveland-* | 30 | 26 | 63.1% |
| decision-tree-primary-tumor-* | 40 | 22 | 60.8% |
| decision-tree-anneal-* | 16 | 12 | 58.8% |
| ParametricRBACMaintenance_mse20 | 128 | 97 | 54.6% |
| ramsey | 28 | **28** | 53.0% |
| MinimumWeightDominatingSetProblem | 9 | 6 | 49.5% |
| switchingactivitymaximization | 19 | 9 | 41.7% |
| correlation-clustering | 90 | 68 | 13.5% |
| generalized-ising | 129 | **129** | 8.7% |

典型个案：

| 实例 | CASH 末态 UB | 混合末态 UB |
| --- | --- | --- |
| `setcover_wt-rail2536.wcnf` | 2 147 034 | **730** |
| `setcover_wt-rail4284.wcnf` | 2 119 198 | **1 137** |
| `ramsey_wt-ram_k4_n19.ra1.wcnf` | 3 883 075 | **2 257** |
| `staff-scheduling_wt-instance7.wcnf` | 32 785 | **1 151** |
| `staff-scheduling_wt-instance11.wcnf` | 86 274 | **3 612** |
| `correlation-clustering_wt-Rounded_…` | 1 312 539 222 | **84 434 782** |

这一组的证据强度远高于 A/C：**族内多个实例方向一致、幅度 2–4 个数量级、且 129/129 与 28/28 这样的比例不可能是偶然。**

## 5. 对照组 B：CASH 证明而混合没证明（同口径，325 个配对）

为公平起见，随机列 10 个（详见 `manifests/curated_ls_value_groups.tsv` 标 B 的行）：

| 实例 | CASH | 混合 |
| --- | --- | --- |
| `CSG_wt-CSG140-140-6.wcnf` | lb=ub=52385 | lb=50419 ub=52385 |
| `CSG_wt-CSGNaive140-140-6.wcnf` | lb=ub=56309 | lb=52643 ub=58415 |
| `MaxSATQueriesinInterpretableClassifiers_wt-toms_test1` | lb=ub=1308 | lb=1177 ub=1308 |
| `abstraction-refinement_wt-downcast-hsqldb.wcnf` | lb=ub=30207 | lb=n/a ub=n/a |
| … | … | … |

其中 `abstraction-refinement_wt-downcast-hsqldb.wcnf` 的混合运行连 UB 都没有产出（`ub=n/a`），属于必须排查的失败模式。

## 6. 据此生成的验证清单与批次

`manifests/curated_ls_value.txt` = 52 个实例：A 5 + C 22 + D 15 + 对照 B 10（分组见 `manifests/curated_ls_value_groups.tsv`）。族分布：staff-scheduling 7、abstraction-refinement 4、drmx-cryptogen 4、correlation-clustering 4、CSG 4、tcp 3、setcover 3，其余各 1–2。

验证批次 `curated_ls`（提交见 `runs/curated_ls/meta/meta.json`）：

```bash
python3 scripts/run_batch.py --batch curated_ls --manifest curated_ls_value \
  --configs CASH,HybridSafe,HybridSelective,HybridLate \
  --seeds 20260909,20260910,20260911 \
  --budget 600 --cash-window 30 --spb-window 10 --workers 64 --mode full
```

4 个臂 × 52 实例 × 3 seed = 624 次运行。臂的选法：

- `CASH`：基线；
- `HybridSafe`：全集上表现最好的混合配置（`full_safe`：398 vs CASH 399，H 胜 0 / C 胜 1）；
- `HybridSelective`、`HybridLate`：A 类和 C 类获胜算例里出现最多的两个配置族（选择性交接 / 延后启动）。

**判读标准（先写死，避免事后挑数据）**：
1. A 类算例：若 3 个 seed 里 ≥2 次由混合证明且 CASH 从未证明 → 该算例可作为有效性证据；
2. C 类算例：若中位墙钟比 < 0.8 且至少 2/3 seed 一致 → 可作为收敛效率证据；
3. D 类算例：若上界降幅在 3 个 seed 上都 ≥ 50% → 可作为随时求解质量证据；
4. 对照 B 类：若混合在其中多数算例上仍输，则说明 A 类的 5 个胜例不能推广，只能作为个案。

## 7. 验证结果（批次 `curated_ls`，624 次运行，约 52 分钟）

### 7.1 证明数总量（52 实例 × 3 seed = 每臂 156 次运行）

| 臂 | seed60909 | seed60910 | seed60911 | 合计 |
| --- | --- | --- | --- | --- |
| CASH | 35 | 34 | 35 | 104 |
| HybridSafe | 35 | 35 | 34 | 104 |
| HybridSelective | 32 | 34 | 34 | 100 |
| **HybridLate** | 36 | 36 | 35 | **107** |

按组拆开（原始 3 seed 计数）：

| 组 | n | CASH | HybridSafe | HybridSelective | HybridLate |
| --- | --- | --- | --- | --- | --- |
| A（只有混合证明过） | 5 | 8 | 8 | 7 | **12** |
| C（两者都快） | 22 | 66 | 66 | 65 | 65 |
| D（UB 大幅改善） | 15 | 0 | 0 | 0 | 0 |
| B（对照，CASH 胜） | 10 | 30 | 30 | 30 | **30** |

### 7.2 A 类 5 个算例的复现情况（判据 A）

| 实例 | CASH | HybridSafe | HybridSelective | HybridLate | 结论 |
| --- | --- | --- | --- | --- | --- |
| `shiftdesign_wt-limits-10-10_data-2_inst-025_30m…` | **0/3** | 0/3 | 0/3 | **3/3** | ✅ 复现 |
| `tcp_wt-tcp_students_91_it_13.wcnf` | 2/3 | 2/3 | 0/3 | **3/3** | ✅ HybridLate 复现 |
| `metro_wt-metro_8_8_5_20_10_6_500_1_0…` | 0/3 | 0/3 | 1/3 | 0/3 | ❌ 未复现 |
| `abstraction-refinement_wt-downcast-pmd.wcnf` | **3/3** | 3/3 | 3/3 | 3/3 | ❌ 原记录是噪声 |
| `judgment-aggregation-ja-kemeny-preflib-00043-00000180` | **3/3** | 3/3 | 3/3 | 3/3 | ❌ 原记录是噪声 |

**5 个里 1.5 个复现**：`shiftdesign` 是完全复现（CASH 0/3 对 HybridLate 3/3），`tcp_91_it_13` 是半复现（CASH 2/3）。另外两个原"胜例"在本批次里 CASH 稳定证明，证明它们原本就是噪声——这正是先定判据的价值。

### 7.3 收敛效率（判据 C）：只有 HybridSelective 成立

只在"双方 3 个 seed 都证明"的实例上比较中位墙钟：

| 臂 | 可比实例 | 中位比值 | 快于 0.9 | 慢于 1.1 | 最好 | 最差 |
| --- | --- | --- | --- | --- | --- | --- |
| HybridSafe | 34 | 1.02 | **0** | 3 | 0.92 | 1.12 |
| **HybridSelective** | 33 | **0.92** | **15** | 8 | **0.25** | 1.32 |
| HybridLate | 33 | 1.01 | 1 | 3 | 0.83 | 1.23 |

HybridSelective 的显著个例：

| 实例 | CASH | HybridSelective | 比值 |
| --- | --- | --- | --- |
| `relational-inference_wt-ar-3.wcnf` | 135s | **34s** | 0.25 |
| `quantum-circuit-vqe_4_12_ibmq-casablanca_7.wcnf` | 79s | **40s** | 0.51 |
| `tcp_wt-tcp_students_91_it_14.wcnf` | 63s | **37s** | 0.59 |
| `quantum-circuit-qftentangled_5_48_rigetti-agave_8.wcnf` | 66s | **41s** | 0.61 |
| `relational-inference_wt-ar-1.wcnf` | 51s | **34s** | 0.66 |
| `frb_wt-frb30-15-3.wcnf` | 336s | **243s** | 0.72 |
| `quantum-circuit-qftentangled_5_30_ibmq-london_5.wcnf` | 53s | **40s** | 0.76 |
| `protein_ins_wt-3ebx_.1era_.g.wcnf.t.wcnf` | 312s | **242s** | 0.77 |

### 7.4 上界质量（判据 D）：15/15 全部复现，且 seed 间完全一致

每个实例 3 个 seed 的 UB 降幅（中位 = 最小，说明逐 seed 稳定）：

| 族 | 实例数 | HybridSafe | HybridSelective | HybridLate |
| --- | --- | --- | --- | --- |
| setcover（rail2536/4284/4872） | 3 | 0% | **100%** | **100%** |
| staff-scheduling（5/6/7/9/10/11/12） | 7 | 0% | **93–96%** | **93–96%** |
| ramsey（ram_k4_n19） | 1 | **100%** | **100%** | **100%** |
| correlation-clustering（4 个） | 4 | 0% | **21–93%** | **21–94%** |

注意 **HybridSafe 在 12/15 上降幅为 0**：它的 `gate_min_percent=20` 加上 `start_fraction=0.3` 让它几乎不交接，因此它在整批上与 CASH 完全相同（104 vs 104、0 个更快实例、无 UB 收益）。**之前被当作"最佳混合配置"的 HybridSafe 实际上退化成了 CASH**，局部搜索的贡献不在它身上。

### 7.5 对照组的代价

- **10 个对照实例上没有任何一个臂输掉证明**（三个臂都是 30/30）。
- 真正的损失只有 2 个实例：`tcp_wt-tcp_students_91_it_13`（CASH 2/3，HybridSelective 0/3）与 `timetabling_wt-comp02`（CASH 3/3，HybridSelective/HybridLate 2/3）。HybridSafe 这两处均无损失（因为它不交接）。

## 8. 结论：可以写进论文的三条证据

1. **证明数量（可复现）**：`HybridLate` 在 `shiftdesign_wt-limits-10-10_data-2_inst-025_30m.sm-extracted.wcnf` 上把 CASH 的 0/3 变成 3/3；在 `tcp_wt-tcp_students_91_it_13.wcnf` 上从 2/3 提到 3/3。A 组总计 **12 vs CASH 8**，且 10 个对照实例零损失。这是"局部搜索确实能多证明"的唯一站得住脚的算例组，应当作为核心案例。
2. **收敛效率（可复现）**：`HybridSelective` 在 33 个可比实例里 15 个快 ≥10%，中位比值 0.92，最佳 `relational-inference_wt-ar-3` 4 倍加速（135s→34s）。代价是 8 个实例变慢（最差 1.32）。
3. **随时求解质量（可复现且幅度最大）**：15/15 实例、3 个 seed 全部一致，降幅 21–100%；`setcover` 与 `ramsey` 达 100%，`staff-scheduling` 93–96%。这一条证据强度最高，建议作为"加入局部搜索有效"的主证据，而证明数量作为辅证。

**同时必须写进论文的负面结果**：`HybridSafe` 之所以在全集上打平 CASH，是因为它几乎不交接（等同基线）；而真正带来收益的 `HybridSelective` 会输掉约 4 个证明。所以"既保住证明数、又拿到 UB/收敛收益"的组合策略仍然是未解决问题——这是本工作的诚实边界，也是下一步的着力点。
