# MSE 2026 无权重（UNWEIGHTED）赛道算法与实现调研

**调研日期**：2026-09-12
**数据来源**（全部为官方原文，非二手转述）：

- *MaxSAT Evaluation 2026: Solver and Benchmark Descriptions*（2026 官方论文集草稿，74 页，University of Helsinki Series of Publications B，报告号 B-2026-??）
  <https://maxsat-evaluations.github.io/2026/>
- EXACT-UW 排名页、ANYTIME-UW 排名页、Tracks 页、EXACT-UW Solver Instances Per Family 页（同上站点 2026 目录）

**本文回答的问题**：无权重 MaxSAT 里"权重分层（stratification）"还有没有用？2026 年无权重赛道的参赛方案实际在用什么？以及这些方案对本项目"融合算法优于 CASH"这一目标意味着什么。

---

## 0. 结论先行

### 0.1 无权重赛道下"权重分层"基本退化，近乎无用

有两条独立的硬证据。

**第一条：赛道定义本身就排除了权重层次。** 官方 Tracks 页对两个精确赛道的定义是：

> **Unweighted**：合并此前评测中的 industrial / crafted 无权重及无权重 partial MaxSAT。
> **Weighted**：合并加权赛道，**"All benchmarks will be truly weighted, i.e., contain soft clauses with different weights."**

也就是说，EXACT-UW 里的软子句按构造权重全为 1（无权重 partial MaxSAT）。输入层面根本不存在"层"——没有 w(l₁)=10、w(l₃)=1 这种量级差异可供分层算法利用。分层的对象只能是求解器**自己造出来的**权重。

**第二条：作者自己承认在无权重实例上分层信号为零。** RustiMax（ANYTIME-UW 第 13 名 rustimax-c、第 15 名 rustimax-r）的求解器描述原文：

> "On detected MWDS instances the UB seeded here is, in our internal benchmarks, tighter than the SLS warmstart, because **pure OLL extracts mostly weight-1 cores that carry no stratification signal**."

这是 2026 年 OLL 系求解器作者对"无权重 OLL 里分层没有信号"的直接陈述。

**第三条（结构性）**：在 OLL/PMRES 系算法中，无权实例里每次核心的权重都是 1。核心 C 的权重 = min{rw(l) : l ∈ C} = 1，松弛后该核心的基数约束权重为 1，软子句残余权重归 0。于是"分层"退化成"按某个顺序逐个解锁核心"——这正是朴素 OLL 的默认行为，分层不引入任何额外信息。同理，EvalMaxSAT 2026 新加的 "Cardinality Weight Reorganization via Linear Programming"（用 LP 重排核心引导过程中产生的基数约束权重）在无权实例上也是一个恒等映射：所有待重排的权重都是 1。

### 0.2 分层真正起作用的地方是"真加权"和 BMO

WMaxCDCL2026（EXACT-UW 第 3 名 WMaxCDCL2026S6O3，417 题）确实实现了 "Stratified Hardening Strategy"，但描述里写得很清楚：

> "Inspired by stratification techniques in core-guided algorithms, WMaxCDCL2026 implements a preprocessing strategy to **exploit the hierarchical structure in WPMS instances**. The strategy consists of hardening high-weight soft clauses to simplify **Boolean Multilevel Optimization (BMO)** instances."

而且它靠这个策略多解出来的是 `frbanddrmx-cryptogen` 这类**带层次权重的**族。同一批作者提交的**无权重**版本 MaxCDCL2026 **没有**这个策略——它的三个新点全部是权重无关的下界/编码技术（增量下界、增量基数编码、further lookahead）。这是"分层只在加权侧有用"的对照实验式证据。

### 0.3 那无权精确求解器靠什么取胜

靠**下界质量**与**下界推进效率**，而不是上界：

1. 更强的下界计算：BnB 局部核心 + lookahead（MaxCDCL 系）、增量下界跨决策层复用；
2. 更聪明的问题改写：增量基数约束编码、核心耗尽（exhaustion）、核心最小化、不相交核心、AtMost1 预处理；
3. 外部数学规划/组合优化后端做初值上界与剪枝：SCIP（UWrMaxSAT 2.0、WMaxCDCL-S6、MaxCDCL-S2、MaxHS、COGS、CoreForge-ILP）、CP-SAT；
4. 多策略并行组合（portfolio）：UWrMaxSAT 与 SCIP 双线程互换 LB/UB；Open-WBO 前 300s + MaxCDCL-S2 后 3300s；
5. 局部搜索只用来**快速压低上界**，用于缩小精确阶段的搜索空间和硬化阈值触发点，用完即走。

### 0.4 与本项目直接相关的三条

- EXACT-UW 上，本项目的基线 **CASHWMaxSAT 414 题，第 5 名**；第一名 **UWrMaxSAT2026 424 题**，两者差 10 题，VBS 477。
- CASHWMaxSAT 落后的族高度集中：`minimizing-pentagons`（0/15，对手 14–15）、`asp-optimization`（4/20，MaxHS 20、EvalMaxSAT 19）、`single-machine-scheduling`（6/15，MaxCDCL2026S2O3 11）、`inconsistency-measurement`（2/15，UWrMaxSAT2026 7）、`frb`（5/5 但 CASHWMaxSATGoal 只 2/5）。这些都是**下界/结构化推理**的短板，不是上界短板。
- 局部搜索的当代正确用法（见 §4）：**入口做 UB 与相位种子 → 立刻交还控制权给精确阶段**；是否给 LS 时间用**离线学习的实例特征预测**来门控（NuWLS-c-2026 的 LightGBM 预测器），而不是定长窗口或固定阈值。这条正好是本项目"SPB 投入收益"问题的 2026 年标准答案。

---

## 1. 赛道与评测口径

| 项目 | 内容 |
| --- | --- |
| 届次 | 第 20 届（2006 起，2025 年停办一年），SAT 2026 卫星会议 / FLoC Olympics 2026 |
| 赛道总数 | 5：精确 unweighted、精确 weighted、anytime unweighted、anytime weighted、增量 |
| 精确赛道时限 | 每题 1 小时 |
| anytime 时限 | 每题 **60s / 300s / 1707s** 三档分别排名（1707s 为提交截止后从 900–1800s 随机抽取的时限；所有 anytime 选手须能撑到 1800s） |
| 开放性 | 求解器必须开源；须提交 1–2 页描述 |
| 随机实例 | 2026 **不设**随机实例赛道 |
| 精确赛道排序依据 | n-solved（解出题数，主指标）→ avg-time → par-2 |
| anytime 排序依据 | 分数（三档各自独立），VBS 为理想虚拟最优 |
| 取消资格 | ANYTIME-UW 中 **HWS** 与 **PX-TT-Open-WBO-Inc** 因产生至少一个错误结果被取消资格（仍列出分数，带 `*`） |

---

## 2. 表 1：EXACT-UW 完整排名（22 个提交配置 + vbs 参考线）

| # | 求解器 | 解出题数 | 平均耗时 (s) | par-2 |
| --- | --- | --- | --- | --- |
| — | **vbs（虚拟最优）** | **477** | 174.40 | 1679.06 |
| 1 | UWrMaxSAT2026 | **424** | 341.39 | 2409.15 |
| 2 | UWrMaxSatMaxCDCL | 423 | 361.81 | 2434.67 |
| 3 | WMaxCDCL2026S6O3 | 417 | 346.29 | 2491.60 |
| 4 | MaxCDCL2026S2O3 | 415 | 278.61 | 2467.92 |
| 5 | **CASHWMaxSAT（本项目基线）** | **414** | 437.18 | 2587.47 |
| 6 | EvalMaxSATSCIP | 409 | 429.61 | 2638.07 |
| 7 | MaxCDCL2026S2 | 407 | 243.58 | 2535.64 |
| 8 | CASHWMaxSATInc | 406 | 409.92 | 2658.37 |
| 9 | CASHWMaxSATGoal | 406 | 402.09 | 2653.13 |
| 10 | EvalMaxSAT | 404 | 304.39 | 2610.50 |
| 11 | EvalMaxRes | 404 | 480.12 | 2727.46 |
| 12 | COGS | 403 | 439.89 | 2711.82 |
| 13 | MaxCDCL2026 | 401 | 215.80 | 2586.06 |
| 14 | WMaxCDCL2026S6 | 398 | 438.13 | 2766.35 |
| 15 | WMaxCDCL2026 | 391 | 293.16 | 2750.95 |
| 16 | MaxHS | 374 | 276.47 | 2934.10 |
| 17 | CoreForgeILP | 372 | 219.36 | 2921.92 |
| 18 | CoreForgeILPLA | 370 | 248.31 | 2962.56 |
| 19 | CoreForge | 357 | 172.02 | 3066.57 |
| 20 | MaxHSScipExact | 340 | 225.62 | 3293.43 |
| 21 | CPMpy_HiGHS | 215 | 319.91 | 4763.06 |
| 22 | CPMpy_ORTools | 213 | 422.08 | 4821.59 |

> 说明：CASHWMaxSAT / Inc / Goal 是同一核心的三个配置；WMaxCDCL 与 MaxCDCL 各三个配置（baseline / +SCIP / portfolio）；CoreForge 三个配置（baseline / ILP / ILP+lookahead）。同核心多配置说明**工程组合（portfolio）带来的收益（如 CASHWMaxSATGoal、MaxCDCL2026S2O3）与算法创新同量级**。

---

## 3. 表 2：EXACT-UW 代表求解器机制对比

出处页码为该求解器在 MSE 2026 论文集草稿中的起始页（Contents 所载页码）。

| 求解器（排名） | 核心算法范式 | 是否用局部搜索 / 如何使用 | 分层或权重相关机制（在 UNWEIGHTED 下是否有效） | 其他关键技术 | 出处 |
| --- | --- | --- | --- | --- | --- |
| **UWrMaxSat2026** (1, 424) | 核引导 **OLL**（默认）+ SCIP 并行双线程互换 LB/UB | 本体不含 LS；同族伙伴配置才可选 SPB 做 UB 探测 | **GBMO**（generalized Boolean multi-level optimization）：把 SCIP 目标函数按分层切点分段优化——**加权/BMO 专用**，无权实例无切点可分；无权精确赛道只用 MaxPre（60s） | CaDiCaL 2.1.3；可参数化的核心最小化（按冲突数限流）；fixed-variable 共享；SCIP 延迟启动以先算出 LB/UB 再交付 | p.24 |
| **UWrMaxSat-MaxCDCL** (2, 423) | UWrMaxSat（MaxPre + 延迟 SCIP）+ **MaxCDCL 顺序兜底** | 源码含**可选** SPB-MaxSAT 3 作为 MaxCDCL 的 UB 提供者 | 无分层；策略是"剩余 LB–UB 间隙仍大时切 MaxCDCL" | 触发条件明确：延迟 SCIP 阶段 + 继续 UWrMaxSat 搜索后间隙仍大 | p.28 |
| **WMaxCDCL2026S6O3** (3, 417) | **CdCL 风格 BnB** + Open-WBO 组合 | 无 LS；用 SCIP 求初值 UB | **Stratified Hardening**：按权重降序硬化高权软子句并调 SAT oracle 验证，专为 **BMO/WPMS** 设计，受益族 `frbanddrmx-cryptogen`。无权版 MaxCDCL2026 不含此项 | **Weight-Aware Lookahead**：优先传播高残余权重软文字、穷尽单元传播、构造高权核心（权重严重不均时显著强于经典 lookahead）；组合：Open-WBO 300s + WMaxCDCL-S6 3300s；SCIP 600s | p.29 |
| **MaxCDCL2026S2O3** (4, 415) | CdCL 风格 **BnB**（无权重专用） | 无 LS | 无分层（这正是对照：无权重版只做权重无关技术） | ①**增量下界**：局部核心带 reason 与决策层，回溯到层 k 时小于等于 k 的核心保留复用；②**增量基数约束编码**：只在出现 pure soft conflict 或 ¬ℓ₁+…+¬ℓₙ = UB−1 时按需加 Sinz 子句（n≤200 且 n×(UB−1)≤10000）；③**Further Lookahead (FLA)**：对 w(κ)=1 小核心逐文字假设求新核心，把 UB 增 2；SCIP 200s | p.21 |
| **CASHWMaxSAT** (5, 414) | 核引导 + **不相交核心** + 权重分布假设 + SCIP | （本项目即在此基线上接 SPB） | 按权重分布的假设策略——无权实例下权重分布退化为单点 | 2026 新增：①**目标界编码**（先用纯硬子句求可行解作初值 UB，再把目标约束编码进硬约束，排除不优于初值 UB 的赋值）；②**Solution-Improving Search (SIS)**：反复搜索目标值更小的可行解，每次改进即收紧 UB 并更新目标约束 | p.10 |
| **EvalMaxSATSCIP** (6, 409) / **EvalMaxSAT** (10, 404) | **OLL**（源自 MSCG/RC2） | **内置局部搜索**（2026 新）：只求"尽快到一个局部极小"，随后立刻交还核引导。理由是外部 NuWLS 有启动开销且无法共享传播信息 | **Cardinality Weight Reorganization via LP**（2026 初步实现）：用 LP 重排核心引导中产生的基数约束权重以缩小后续核心、提早硬化。**无权实例下所有相关权重都是 1，重排无意义** | 架构重构（组件接口隔离，便于插拔策略）；SCIP 配置与 baseline 差 5 题，说明组合仍占优 | p.18 |
| **EvalMaxRes** (11, 404) | EvalMaxSAT + **MaxSAT resolution（PMRes）** | 同 EvalMaxSAT | 加权时仍照传统核引导抽最小权重 | **按核心大小切换核心处理策略**：|C| ≤ 10 用 PMRes 的 MaxSAT resolution 变换，否则用 OLL 基数约束法；用二元硬子句 l₁∨d₁ 代替长子句 | p.19 |
| **COGS** (12, 403) | CGSS2（OLL 的改进：精化核心松弛 + 权重感知核心抽取 + 基数约束结构共享）+ **有序目标** | 无 LS | **Ordered Objective Stratification**：构造活跃目标假设的蕴含图 G_A，边 a→c 表示假设 a 下的单元传播能推出 c。仅当该 DAG 全序（成链）时才启用有序分层；链上按 SimpleSIS 同序解锁。**但分层依据是"目标变量间的蕴含序"，不是权重** | 图式核心简化引理：若 F ⊨ p→q 则 C\{q} 仍是核心（仅用可证蕴含，与分层无关，任意实例有效）；SCIP prerun 提供模型与 UB；用 UWrMaxSAT 后处理；Glucose 4.1 | p.13 |
| **MaxHS** (16, 374) | **隐式命中集 IHS**（portfolio：CaDiCaL 抽核心 + SCIP 优化器求最小代价命中集） | 无 LS | 无权重分层；IHS 天然不依赖权重层次 | 主要工作是稳健性：接入 SCIP 10.0.2 取代 CPLEX；**exact mode** 用 SCIP 数值稳定模式（否则大权重下 SCIP/CPLEX 有数值问题）；MaxPre 2.2；CaDiCaL 3.0（关掉 ILB、Lucky Assumptions、Factoring） | p.23 |
| **CoreForge** (17/18/19) | **LLM 生成的 OLL** 核引导（25K+ 行 C++，全部由 Codex/GPT 生成，人工不写码） | 无 LS（不完整法靠 backend） | 无分层；AtMost1 预处理、不相交核心预处理、核心耗尽、核心最小化 | ILP 配置：纯硬子句 SAT 求初值 UB + 轻量变异的 UB 改进 + SCIP presolve 60s + SCIP 90s + CP-SAT 90s + 短 MSU3（>500K 子句时关闭 ILP 后端）；lookahead 配置：**core-sequence lookahead**（用不同假设排序/早期核心处理策略做有界探针，按轻量统计打分选策略再重启主搜索） | p.15 |
| **RustiMax**（提交于 **ANYTIME-UW**，非精确赛道；#13 rustimax-c、#15 rustimax-r） | **OLL + stratification & hardening**（Rust 从零实现；rustimax-c 用 CaDiCaL 1.9.5，rustimax-r 用纯 Rust 的 RustiCal） | **有**：SLS warmstart（NuWLS-c-IBR 的 clean-room 移植），一次限时，成功后**同时**播种 OLL 的 UB **和 SAT 求解器的 preferred-phase 向量**；每次 UB 更新后跑**有界冲突限量的 Hamming 邻域探针** | 明确承认无权重下分层无信号（见 §0.1 引文）。因此为 MWDS 实例另写**族特定 primal**（贪心 + SAT 验证的最小权覆盖） | 7 步流水线：fast path（num_clauses/timeout > 200000 时跳过预处理）→ SLS warmstart → **AM1 预处理**（三种 BCP 驱动方法：精确最大团、fast probe、very-fast probe，加 AM2 rank-2 推广）→ UB seed（只在纯硬子句上跑独立 SAT，学到的子句不并入 OLL，避免污染）→ shared-solver fallback seed → 族特定 primal → OLL 主循环；三重安全网（三阶段 stall 救援、random-feasible 兜底、首个外层求解 150s 上限）；Lambert-W 自适应分配核心间算力；每轮迭代输出 LB 轨迹 | p.26 |
| **CPMpy_HiGHS / ORTools** (21/22) | 把 WCNF 当中间表示，翻译到 ILP (HiGHS) / LCG CP (OR-Tools CP-SAT) | 无 | 无 | 定位是"拓宽评测的技术面"，非竞争方案；解出 213–215 题 | p.11 |

### 3.1 表 2 的两个要点

1. **分层系技术全部出现在加权侧**：WMaxCDCL2026 的 stratified hardening（WPMS/BMO）、UWrMaxSat 的 GBMO（多级目标）、EvalMaxSAT 的基数权重 LP 重排（加权核心权重才不等于 1）。无权重侧的三位主角（UWrMaxSAT2026、MaxCDCL2026、MaxHS）都不用分层。
2. **唯一在无权重下仍以分层命名的机制是 COGS 的有序目标分层**，而它的分层依据是**蕴含图的全序**，与权重无关——本质是把"按目标变量顺序逐个解锁"的形式化成图论判据，顺带提供了一个与分层无关的核心简化引理（这在无权实例上一样有效）。

---

## 4. 表 3：ANYTIME-UW 排名与"局部搜索如何被耦合"

ANYTIME-UW 分数（60s / 300s / 1707s 三档）：

| # | 求解器 | 60s | 300s | 1707s | LS 耦合方式 |
| --- | --- | --- | --- | --- | --- |
| — | **vbs** | 0.9245 | 0.9615 | 0.9941 | — |
| 1 | **PASMaxSAT-c** | 0.8606 | 0.9049 | 0.9476 | 改自 TT-Open-WBO-Inc；SPB 作用于**全量实例**（渐进子问题扩张因收益不稳被放弃）；SPB 内部**多轮重启** 3→10 轮（上轮有改进则 +1），上轮可行解作为下轮初值，否则随机重启；间隔 >200s 且当前最好解优于上轮 LS 结束时最好解的 95% 才**再次进入** SPB |
| 2 | **SGAT-NuWLS-c** | 0.8577 | 0.9031 | 0.9434 | NuWLS-c-2023 + SAT-based Graph Attention Network：**用 GNN 预测变量赋值来替换原 SAT 初值**，再做局部搜索；SGAT 在 C++ 中从 Python 移植实现，模型权重运行时加载；训练数据为 2018–2020 MSE 实例（排除 partial） |
| 3 | **Aperture-GBG** | 0.8563 | 0.8990 | 0.9444 | 统一流水线：SAT 初解 → **局部搜索（默认 SPB-Band，亦支持 USW、DeepDist）** → 不完全 SAT 优化（MrsBeaver，内含 Polosat）→ 完整 LSU 阶段。关键控制：**限制 MrsBeaver 连续无改进迭代数**，超限即强制转入完整 LSU，避免在近似阶段空耗 |
| 4 | **NuWLS-c-2026-UB** | 0.8548 | 0.8981 | 0.9330 | 同 NuWLS-c 系；新增**轻量单元传播预处理** + LinearSU 聚类框架内的**冲突预算**（核心引导精化阶段完全解除预算以保证精确） |
| 5 | Aperture-KBG | 0.8527 | 0.9069 | 0.9458 | 同上，换 SAT 后端（AE-Kissat-MAB） |
| 6 | Aperture-IBI | 0.8411 | 0.8892 | 0.9378 | 同上，IntelSAT 后端 |
| 7 | **DeepDist-c** | 0.8079 | 0.8506 | 0.9047 | DeepDist（**Deep-Weighting** 深度区分子句权重 + UnH-Decimation）作 LS 引擎 + TT-Open-WBO-Inc 作 SAT 引擎；注意只有 Deep-Weighting 被集成进混合器，UnH-Decimation 未集成 |
| 8 | **NuWLS-c-2026** | 0.8067 | 0.8608 | 0.9087 | **可实例感知的 LS 预算分配**（本年度最重要的调度创新，见 §4.1） |
| 9 | BouMS-CaDiCaL | 0.7707 | 0.8009 | 0.8422 | NuWLS-2.0 + **LFF（least-frequently-flipped）** 逃逸机制 + **IDP**（实例相关精度的定点整数代替浮点）；CaDiCaL 跑一次仅求可行初值（该配置关闭 decimation 以保持可行解） |
| 10 | HWS*（**取消资格**） | 0.7529 | 0.7724 | 0.7828 | 历史最优状态引导 + **滑窗控制子句加权**（t−t_best ≥ δ 才增权，δ=5，h_inc=s_inc=1） |
| 11 | BouMS | 0.7348 | 0.7618 | 0.7832 | 纯无依赖实现（无动态内存、无浮点、循环全可静态上界），面向嵌入式实时系统 |
| 12 | **Loandra** | 0.7046 | 0.8360 | 0.8990 | **core-boosted linear search**：预处理 → 核引导 CG（PMRES + 权重感知核心抽取 + 分层 + 子句硬化）→ SIS（SAT/UNSAT 线性搜索 + DPW 编码）→ **SLS 周期性介入**：在每次"新 resolution"（即用更大的软子句集重新初始化 SIS）开始时调用；目标是给出好上界以**缩小 SIS 要编码的 PB 约束规模**，并修正预处理导致的"代价容易被局部改动改善"的解。**无权重实例上核引导阶段最迟抽完一组不相交核心即终止** |
| 13 | Rustimax-c | 0.6318 | 0.7101 | 0.7579 | 见 §3（SLS 播种 UB + 相位向量，UB 更新后跑 Hamming 探针） |
| 14 | PX-TT-Open-WBO-Inc*（**取消资格**） | 0.6252 | 0.6505 | 0.6915 | Aperture 的前身 |
| 15 | Rustimax-r | 0.6034 | 0.7004 | 0.7510 | 同上，纯 Rust SAT 后端 |
| 16 | CPMpy_HiGHS | 0.2595 | 0.3397 | 0.3628 | — |
| 17 | CPMpy_ORTools | 0.1715 | 0.3404 | 0.5040 | — |
| 18 | PRINTEMPS | 0.1617 | 0.1848 | 0.2110 | 通用 ILP 元启发式（加权 Tabu 搜索）；每个 WCNF 子句译为一个 0–1 不等式，软子句加一个下侧松弛变量 y⁻ 与惩罚 w·y⁻ |

### 4.1 年度最重要的调度创新：NuWLS-c-2026 的实例感知预算分配

原文要点：

> "Allocating an appropriate amount of computational budget to the local search component is a challenging task. To address this issue, we employ a lightweight machine learning technique to learn an offline prediction model based on general structural features of MaxSAT instances."

- 求解前抽取公式级特征（变量数、子句数、硬/软子句数、子句变量比、子句长度统计、**权重统计**等，设计部分借鉴 SATZilla），用 **LightGBM 回归**（离线训练于 MSE 2018–2022 anytime 赛道）预测"追加 LS 算力是否有用"的分数 p_LS；
- 按 p_LS 分三档：**Conservative（直接关闭 LS） / Default（NuWLS-c-2023 的默认预算） / Enhanced（延长 LS 时限并增加重启次数）**；
- 所有阈值与缩放因子在评测前静态写死在源码里，不使用实例名、不手工定义族标识、不做窄目标触发；
- 加权/无权、三档时限共用同一套参数。

这正对应本项目此前的困境："SPB 到底是定上限时间还是定优化目标"。2026 年的答案既不是定时间也不是定目标，而是**先判断这条实例值不值得给 LS 任何时间**，再在同一框架内放大或缩小预算。

---

## 5. 表 4：各类"层级/权重"机制在 UNWEIGHTED 下的可用性判定

| 机制 | 代表求解器 | 依赖什么 | 无权重实例下是否有效 | 依据 |
| --- | --- | --- | --- | --- |
| 权重分层（stratum）解锁核心 | OLL 系全体、Loandra | 软子句权重的最小值序列 | **无效**：每次核心权重恒为 1，退化为一核心一解锁 | 赛道定义 + RustiMax 原文 |
| 分层硬化（stratified hardening） | WMaxCDCL2026、Loandra（默认开，评测提交里也照抄） | 权重降序 + SAT oracle | **无效**（无权重版 MaxCDCL2026 干脆不实现） | WMaxCDCL 原文（目标为 BMO/WPMS） |
| GBMO 目标分段 | UWrMaxSat 2.0 | 加权多级目标的分层切点 | **无效** | UWrMaxSat 原文（描述为加权/BMO 特性） |
| 基数约束权重 LP 重排 | EvalMaxSAT 2026 | 核心引导产生的基数约束权重分布 | **无效**（待重排权重全为 1；原文亦称"初步实现"） | EvalMaxSAT 原文 |
| 权重感知核心抽取 / 权重感知 lookahead | Loandra(WCE)、WMaxCDCL2026 | 核心内软文字权重差 | **无权重下退化为经典策略**；但在权重规模差异极大的实例上收益显著 | WMaxCDCL 的 Example 1/2 对比 |
| 有序目标分层（蕴含图全序） | COGS | 活跃目标假设间的可证蕴含 | **有效**，且不依赖权重；其附带的图式核心简化引理在任意实例上独立有效 | COGS 原文（AAAI 2026 ordered objectives） |
| 增量下界 / 增量基数编码 / further lookahead | MaxCDCL2026 | 局部核心的 reason 与决策层 | **有效**（权重无关，纯下界技术） | MaxCDCL 原文 |
| AtMost1 / 不相交核心 / 核心耗尽 / 核心最小化 | MaxCDCL、CoreForge、RustiMax | 硬子句上的 BCP 与蕴含 | **有效** | 各原文 |
| SLS 求初值 UB → 播种硬化阈值 | EvalMaxSAT、Loandra、RustiMax、PASMaxSAT-c | 上界质量 | **有效，但收益受"能否触发硬化"限制** | EvalMaxSAT：「只求尽快到局部极小，然后交还核引导，因为它更擅长脱离局部极小和证明最优」 |
| SLS 播种 SAT 相位向量 | RustiMax | UB 对应的赋值 | **有效**（把 LS 的成果转成 CDCL 的搜索偏置） | RustiMax 第 2 步 |
| LS 预算的实例感知门控 | NuWLS-c-2026 | 公式级结构特征 → LightGBM | **有效**，且正是"LS 是否值得投入"的正解 | 见 §4.1 |

### 5.1 无权重下"替代分层"的五种已有做法

如果确实需要在无权重实例上保留"分层"这一形式，2026 年的参赛方案给出了五种可替代的分层依据——**都不依赖软子句权重**：

| 替代分层依据 | 做法 | 代表 | 为什么在无权重下仍有效 |
| --- | --- | --- | --- |
| **决策层（decision level）** | 给每个局部核心一个 level = 其 reason 中最大的决策层；回溯到层 k 时，level ≤ k 的核心全部保留复用，level > k 的丢弃 | MaxCDCL2026 增量下界 | 层次来自 CDCL 的回溯结构，与权重完全无关；直接省掉重复探测核心的时间 |
| **目标变量蕴含序** | 构造活跃目标假设的蕴含图，若成链则按链的反序解锁（等价于 SimpleSIS 的 SAT→UNSAT 方向） | COGS（AAAI 2026 ordered objectives） | 序来自公式的可证蕴含关系 |
| **核心序列（core sequence）** | 用不同的假设排序 / 早期核心处理策略做有界探针，对观察到的核心前缀打分，选最优策略重启主搜索 | CoreForge 的 core-sequence lookahead | 不同早期核心序列会导向结构差异很大的改写，选择本身即收益 |
| **硬子句上的互斥关系** | 检测互相排斥的软文字（AM1：精确最大团 / fast probe / very-fast probe，加 AM2 rank-2 推广），每个团只留一个残余软子句 | RustiMax AM1 预处理 | 直接抬高下界，且是权重无关的纯结构发现 |
| **上界逐级下降** | 反复搜索目标值严格小于当前 UB 的解，每次改进即收紧 UB 并更新目标约束 | CASHWMaxSAT 的 SIS | 上界的"层"由求解器自己制造，与输入权重无关 |

---

## 6. 按实例族的差距分析（EXACT-UW，本基线 vs 前排）

数据来源：官方 *EXACT-UW Solver Instances Per Family* 页。

| 实例族 | 总数 | CASHWMaxSAT | UWrMaxSAT2026 | MaxCDCL2026S2O3 | WMaxCDCL2026S6O3 | MaxHS | EvalMaxSAT | 差距性质 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| **minimizing-pentagons** | 15 | **0** | **0** | 14 | 15 | 0 | **0** | CASH 与纯 OLL/IHS 系全 0；只有 MaxCDCL 系（14–15）与 **UWrMaxSat-MaxCDCL**（15）能解——正是"MaxCDCL 顺序兜底"救回这一族 |
| **asp-optimization** | 20 | **4** | 17 | 11 | 11 | **20** | 19 | IHS(MaxHS) 拿满 20，OLL 系(EvalMaxSAT 19) 领先，CASH 明显落后 |
| **single-machine-scheduling** | 15 | **6** | 4 | 11 | 8 | 5 | 4 | BnB + 增量基数编码受益族（MaxCDCL 系领先） |
| **inconsistency-measurement** | 15 | **2** | **7** | 2 | 2 | 2 | 4 | 只有 UWrMaxSat 系显著领先 |
| **logistics-routing** | 25 | **4** | 1 | 3 | 3 | 1 | 1 | CASHWMaxSAT 反而在此族领先 |
| **frb** | 5 | 5 | 5 | 4 | 5 | 5 | 5 | CASHWMaxSATGoal 配置只解 2，配置敏感性大 |
| **planning-hplus** | 25 | 20 | 15 | 7 | 11 | 12 | 19 | CASHWMaxSAT 与 EvalMaxSAT 领先 |
| **phylogenetic-trees** | 15 | 14 | 15 | 15 | 7 | 9 | 13 | MaxCDCL/EvalMaxRes 领先 |
| **judgment-aggregation** | 15 | 10 | 9 | 10 | 9 | 7 | 6 | 打平 |
| **extension-enforcement** | 10 | 10 | 10 | 6 | 9 | 3 | 4 | CASHWMaxSAT 明显领先 |
| 全部族（合计） | 477 | 414 | **424** | 415 | 417 | 374 | 404 | — |

**可读出的结论**：CASHWMaxSAT 的短板集中在**结构化下界推理**（minimizing-pentagons、single-machine-scheduling、asp-optimization、inconsistency-measurement），其优势集中在 planning/路由/扩展类。10 题的差距不是靠"再优化上界"能补的——因为差距族里的对手（MaxHS 在 asp-optimization 拿满 20 题）恰恰是**不使用任何局部搜索**的 IHS 求解器。

---

## 7. 对本项目（HybridMaxSAT / CASH + SPB）的可执行结论

1. **不要在无权重/近似无权实例上继续投资分层类机制。** 本项目的调试集 50 题只有 5 种不同软权重（中位数），已经接近退化；MSE 2026 甚至把"真加权"写进赛道定义来和近似无权的实例区分开。若继续做分层/硬化方向，应先按"权重层数"过滤实例集，只在层数足够多的实例上评估该模块。
2. **LS 的定位应当收敛为"入口产物 + 相位种子"，而不是"第二次优化机会"。** EvalMaxSAT 的表述最直白：LS 只求尽快到局部极小，然后交还核引导，因为核引导更擅长脱离局部极小和证明最优；RustiMax 则把 LS 的成果同时喂给 UB 和 CDCL 的 preferred-phase 向量。本项目此前的实验数据（UB 确实被改善，但硬化计数 0/50、LB 推进只有纯 CASH 的 82%）与这两条设计理由完全吻合。
3. **把"给不给 SPB 时间"做成可预测的决策。** 直接对标 NuWLS-c-2026：抽公式级结构特征 → 离线回归 → 三档预算（关闭 / 默认 / 加强）。这比"定长窗口"或"定优化目标"更贴近 2026 年的做法，而且天然回答了"SPB 是否值得投入"。
4. **若目标是解出题数而非 UB，把力气放在下界侧。** 无权重侧的三个高杠杆点是：增量下界（跨决策层复用局部核心）、增量/按需基数约束编码、further lookahead 式的"小核心合并成大核心"。第 5 名与第 1 名的 10 题差距，族分布全部指向这里。
5. **组合/portfolio 的性价比不可忽略。** 官方数据里同一核心的不同配置间最多差 ~20 题（CoreForge 357 → 372；WMaxCDCL2026 391 → S6O3 417；MaxCDCL2026 401 → S2O3 415），而 WMaxCDCL2026S6O3 与 MaxCDCL2026S2O3 的加分几乎全部来自"前 300s 跑 Open-WBO、后面跑主求解器"这种朴素分工。
6. **失败也要失败得干净。** ANYTIME-UW 里 HWS 与 PX-TT-Open-WBO-Inc 因一个错误结果就被取消资格。本项目在改调度/门控时，"宁可不改进也不能报错解"的优先级要写成硬约束。

---

## 8. 2026 年这批描述所引用的关键算法论文（本轮被引用的"最新论文"清单）

| 论文 | 出处 | 被谁引用 / 用处 |
| --- | --- | --- |
| Berg, Schidler, Järvisalo. *Ordered objectives in maximum satisfiability* | **AAAI 2026**, vol.40 no.17, pp.14166–14174 | COGS 的有序目标分层与 SimpleSIS 同序解锁的依据（无权重下仍有效的分层机制） |
| Zhang, Li, Cherif, Li. *Weight-aware branch-and-bound for weighted maximum satisfiability* | **IJCAI-26**（to appear） | WMaxCDCL2026 分层硬化与权重感知 lookahead 的实现细节 |
| Zhang, Li, Cherif, Li. *Enhanced lower bound computation in branch-and-bound for MaxSAT* | **CP 2026**（to appear） | MaxCDCL2026 的 further lookahead (FLA) |
| Katsirelos. *Core-Guided Linear Programming-Based Maximum Satisfiability* | **SAT 2025** | EvalMaxSAT 2026 的基数约束权重 LP 重排 |
| Lübke, Berg. *SLS-enhanced core-boosted linear search for anytime maximum satisfiability* | **CP 2025**, pp.28:1–28:20 | Loandra 2026 的 SLS 与核引导紧密集成（含把核心注入 SLS 的开关，评测版关闭） |
| Jiang, Gao, Chen, Chen. *Enhancing local search for MaxSAT with deep differentiation clause weighting* | **ECAI 2025** / arXiv:2512.05619 | DeepDist / DeepDist-c 的 Deep-Weighting |
| Chu, Li, Ye, Cai. *Enhancing MaxSAT local search via a unified soft clause weighting scheme* | **SAT 2024**, 8:1–8:18 | NuWLS 系（含 NuWLS-c-2026）的统一软子句加权 |
| Zheng, Chen, Li, He. *Rethinking the soft conflict pseudo-Boolean constraint on MaxSAT local search solvers* | **IJCAI 2024**, pp.1989–1997 | SPB-MaxSAT / PASMaxSAT-c / UWrMaxSat-MaxCDCL 的 LS 组件（即本项目的 SPB） |
| Paxian, Biere. *MaxSAT fuzzing and delta debugging* | **JAIR 85**, 2026 | MaxHS 2026 修订（用 WCNFuzz 找 bug） |
| Jin, Kuang, Zheng, Mao, He. *PALSAT: Deep Cooperation of Unit Propagation and Local Search in Incomplete SAT Solving* | **SAT 2026**（LIPIcs，本地已有 PDF） | 同一作者群的 PASMaxSAT-c 之外的方法论背景：完整法技术与局部搜索的深度协作 |
| Nadel, Slonimski. *Aperture: an anytime, complete and incremental MaxSAT solver* | 2026，投稿中（未发表） | Aperture 的设计与 TORC 极性 / TSB 变量选择 |

---

## 9. 复现本文数据的命令

```powershell
# 论文集草稿（官方 2026 目录）
Invoke-RestMethod -Uri "https://api.github.com/repos/maxsat-evaluations/maxsat-evaluations.github.io/contents/2026" -Headers @{"User-Agent"="codex"}
# 已下载并抽取正文（270K 字符）到：
#   .codex-tmp/mse26/mse26proc_draft.pdf
#   .codex-tmp/mse26/proc.txt
#   .codex-tmp/mse26/contents_exact_unweighted_ranking.html
#   .codex-tmp/mse26/contents_anytime_unweighted_ranking.html
#   .codex-tmp/mse26/exact-unweighted-by-family.html
#   .codex-tmp/mse26/contents_tracks.html
```
