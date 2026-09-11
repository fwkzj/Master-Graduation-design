# A 线创新点记录：阈值感知的上界协商、相位通道与选择性交接

日期：2026-09-11 ・ 机器：S122（HCCS-122）・ 工作目录：`/home/fwkzj/HybridAlgorithm`

**本文性质**：研究设计记录。记录的是**尚未实现、尚未实验**的候选创新点，用于后续实现与论文写作的输入。
算法与正确性约束仍以 `agent.md` 为准，实验执行规范仍以 `experiment.md` 为准；本文不与它们冲突，
只补充「接下来做什么、为什么这样做、怎么证伪」这一层。

---

## 0. 前提：这些创新点建立在哪些已到手的事实上

A 线不是凭空提出的方向，而是对本项目已有实测结果的直接回应。以下事实全部来自已完成的批次，
数据源为 `runs/iter2_debug50/`（50 实例 × 4 配置 × 3 种子 = 600 次）与 `runs/full2cfg/`（558 实例 × 2 配置）。

### 0.1 证明能力：抢占是唯一的杀手

| 配置 | 证明最优 | `optimum`（CaDiCaL） | `optimum_scip`（ILP） | UB 严格优于 CASH | UB 严格差于 CASH |
|---|---|---|---|---|---|
| CASH | 34/50 | 31 | 3 | — | — |
| Hybrid（抢占，15/3 固定窗口） | 25/50 | 22 | 3 | 8 | 7 |
| **HybridNatural（非抢占）** | **34/50** | **31** | **3** | **6** | **1** |
| HybridAdaptive（非抢占 + 退避） | 34/50 | 31 | 3 | 6 | 1 |

全量批次 `full2cfg` 给出同一结论的放大版：CASH 398/558、抢占式 Hybrid 286/558，
损失 112 全部落在 CaDiCaL 桶，**ILP 路径 37 = 37 毫发无伤**；265 个发生过交接的运行
没有任何一个证明最优，而同样这 265 个实例纯 CASH 证明了 105 个。

**结论**：抢占式时间片（把 CaDiCaL 从一次 SAT 调用里拉出来）会摧毁 CDCL 的证明能力，
这一点已在调试集与全量集上双向确认。`preemptive = false` 的 `HybridNatural` 恢复了证明能力，
在 50 例上与 CASH 持平。**因此 A 线不再讨论是否抢占，只讨论「交接什么」和「什么时候交接」。**

### 0.2 上界价值：SPB 多数时候是提前拿到 CASH 迟早会拿到的界

`runs/iter2_debug50` 的逐轮事件（`events.jsonl`）统计：

| 配置 | 总轮次 | 给出可行解 | 严格改善 UB | 改善占轮次 | 占可行轮 | 至少被改善过一次的实例 | 最终仍领先的实例 |
|---|---|---|---|---|---|---|---|
| Hybrid | 2311 | 1281 | 113 | 4.9% | 8.8% | 19/50 | 8 |
| HybridNatural | 588 | 432 | 62 | 10.5% | 14.4% | **13/50** | **6** |
| HybridAdaptive | 249 | 190 | 54 | 21.7% | 28.4% | 13/50 | 6 |

关键落差：**13 个实例被 SPB 提前改善过，但只有 6 个最终保持领先。** 差额部分不是 SPB 失败，
而是 CASH 后来自己走到了同一个界。这说明当前协议的真实贡献是**在时间维度上提前收紧 UB**，
而不是找到 CASH 找不到的解。主指标「是否证明最优」因此对这种贡献不敏感——这是指标口径问题，
不是方法失效。

### 0.3 机制瓶颈：UB 的边际价值是阶跃的，不是线性的

`CASHWMaxSAT-DisjCad-S6/code/uwrmaxsat/MsSolver.cc:318-321` 的硬化判据是：

    Int UB = (!scip_foundUB ? UB_goalvalue : min(UB_goalvalue, scip_UB));
    Int Ibound = UB - LB_goalvalue, WMAX = Int(WEIGHT_MAX);
    weight_t wbound = (Ibound >= WMAX ? WEIGHT_MAX : toweight(Ibound));
    for (int i = top_for_hard - 1; i >= 0 && soft_cls[i].fst > wbound; i--)
        // hardening soft clauses with weights > the current goal interval length

即：**权重严格大于 `UB - LB` 的软子句被硬化**。由此可得两条推论，它们构成 A1 的全部立足点：

1. UB 每下降一点，未必硬化任何新子句；只有当 `UB - LB` 跨过某个子句权重的下界时，才一次性硬化一批。
   所以「UB 改善量」与「对精确搜索的贡献」之间不是线性关系，中间隔着一个**权重阶梯**。
2. 反之，注入一个**过于激进**的 UB 会让 `UB - LB` 骤降、一次硬化过多子句，可能过度约束搜索。
   「硬化激进度是否存在最优值」是一个尚未被本项目的实验覆盖的问题。

### 0.4 已知边界

- SPB 在超大实例上给不出可行解（`hs-timetabling_wt-BrazilInstance7`，55532 变量，40 秒无解）。
- CASH 传入的传播量很低（同一实例约 2634 / 55532），其余由 SPB 自行随机补全。
- SCIP 能在 15 秒内证出的实例上交接根本不发生（`optimum_scip`），这类运行需在分析中分层。
- 当前正在运行的 `full23w_1seed` 批次（`--configs CASH,Hybrid`）跑的是**抢占式** Hybrid，
  其作用是补齐「按开题协议的严格全量负结果」，不是正面结果。

---

## A1. 阈值感知的上界协商（Threshold-aware upper-bound negotiation）

### A1.1 动因

由 §0.3：SPB 当前的目标函数「最小化 UB」与 CASH 真正在意的东西「让更多软子句被硬化、让 LB 推进」
并不一致。SPB 花 3 秒把 UB 从 1000 降到 990，如果 `UB - LB` 跨过的是空区间，这次交接对精确搜索
的净贡献为零，而代价（CASH 让出的时间）已经付出。

### A1.2 设计

把 `harden_soft_cls()` 使用的权重阶梯暴露成只读信息，交给协调器，再由协调器向 SPB 下发**目标值**：

- CASH 侧新增只读接口，返回「下一个会改变硬化集合的 UB 阈值」。阈值序列就是
  `{LB + w : w 属于 soft_cls 的权重集合，且当前未被硬化}`，取其中最大的那个（即最易达到的改善）。
- 协调器把该阈值随初始解一起交给 SPB，作为**本轮目标**而非硬约束。
- SPB 侧新增「达到目标即可提前返回」的调用变体；未达到时如实回报本轮最佳 UB 与差值。
- 日志新增字段：本轮是否跨过阈值、跨过几个阈值、本次交接新硬化的子句数、硬化后 `UB - LB` 的变化。

### A1.3 实现位置

| 位置 | 改动 |
|---|---|
| `CASHWMaxSAT-DisjCad-S6/code/uwrmaxsat/MsSolver.h/.cc` | 新增只读访问器，返回下一个硬化阈值与已硬化子句数；不改动 `harden_soft_cls()` 的判据 |
| `HybridMaxSAT/include/hybrid_coordinator.h` | `HybridSchedule` 增加 `threshold_aware` 开关；`RoundEvent` 增加阈值相关字段 |
| `HybridMaxSAT/src/hybrid_coordinator.cpp` | `find_upper_bound()` 组装目标值；`on_upper_bound_result()` 记录跨阈结果 |
| `SPBMAXSAT2-master/.../Solver/local_search_solver.h/.cpp` | 新增带目标值的调用变体，复用既有 `persistent_worker_`；不改动局部搜索策略与权重更新公式 |

新配置建议命名 `HybridThreshold`，与 `HybridNatural` 只差目标值下发与提前返回，构成干净的消融对。

### A1.4 可证伪判据

- **H1a**：等量的 UB 改善，跨过硬化阈值的那些比未跨过的带来更多 LB 推进。
  检验：按「本轮是否跨阈」分组，比较轮后 LB 增量中位数。
- **H1b**：存在一个「激进度」最优点。检验：把下发目标设为「下一个阈值」「下 k 个阈值」「全局最优估计」
  三档，比较证明数与 UB 推进速率。
- **H1c**：阈值感知优于固定 SPB 窗口。检验：`HybridNatural` vs `HybridThreshold` 的配对比较。

若 H1a 不成立（跨阈与否对 LB 推进无差别），则 A1 的核心假设被否定，应如实记录并转向 A2/B 线。

### A1.5 风险

- 阈值可能只在部分目标变换分支下成立（`goal_gcd`、`fixed_goalval`、`harden_goalval` 的组合），
  必须先确认接口在所有分支下返回的是**同一内部目标单位**，否则会重复 `agent.md` 明确禁止的错误。
- 硬化子句数不等于搜索收益：硬化既可能带来强传播，也可能带来过度约束。这正是 H1b 要回答的。

---

## A2. 相位通道：SPB 赋值 → CDCL 决策相位

### A2.1 动因

当前 SPB → CASH 的信息通道只有一个标量 UB，是信息量最小的一种交接。而开题报告研究内容（2）
要求的是**双向交互**：目前实际只有 CASH → SPB（传播赋值投影）这一个方向。A2 补上反方向。

### A2.2 设计

在同一个安全点，把 SPB **已验证的最佳模型**（协调器已私有保存的证书）作为原始变量的决策相位写入
CASH 的 SAT 后端，**不改 UB、不改证明责任、不打断搜索**。相位只是启发式偏好，不构成任何承诺，
因此不触碰 `agent.md` 的「不可放宽的约束」——它不进 `best_goalvalue`/`UB_goalvalue`/`LB_goalvalue`，
也不是「某变量必须取某值」的结论。

### A2.3 实现位置（接口已确认存在）

| 后端 | 接口 | 位置 |
|---|---|---|
| COMinisatPS（CASH 主 SAT 引擎） | `setPolarity(Var v, bool b)`，需 `polarity_user` 模式；成员 `vec<char> polarity` | `cominisatps/minisat/core/Solver.h:124`、`:237`、`:492` |
| CaDiCaL | `phase(int lit)` / `unphase(int lit)`；且源码自带重相位机制 `rephase.cpp` | `cadical/src/cadical.hpp:725-726` |

注意两个后端的变量编号包含 CASH 新增的松弛与 sorter 变量，注入时必须**按原始变量前缀**
进行，与 `HybridCoordinator::find_upper_bound()` 已有的截断逻辑使用同一套编号约定（1-based，
元素 0 不用）。

### A2.4 可证伪判据

- **H2a**：相位注入改善首次可行解时间或最终 LB/UB，且不降低证明最优数。
- **H2b**：注入强度存在最优点。检验：只在前 k 轮注入 / 只注入与当前相位不一致的变量 / 按比例注入。
- **H2c**：相位注入与 A1 的阈值协商是互补的还是冗余的（联合消融）。

若 H2a 显示证明数下降，说明相位注入扰乱了相位保存，应退回「仅在前 k 轮注入」。

### A2.5 风险

CaDiCaL 的 `rephase` 会在搜索中周期性重置相位，注入的效果可能被其后继重相位冲掉。
需要在实验中记录注入后到下一次重相位之间的窗口长度，否则容易误判为「无效」。

---

## A3. 选择性交接：只在值得的时候交给 SPB

### A3.1 动因

由 §0.2，交接的成本（让出时间）是每轮无条件支付的，收益（严格改善 UB）只有约 10% 的轮次发生。
最直接的改进是让「是否交接」本身成为一个可决策的量。

### A3.2 设计

不引入黑箱模型，先用**可解释特征**建立判据，且特征必须只依赖运行中已经可观测的量：

- 静态：原始变量数、硬子句数、子句/变量比、权重分布的分位数。
- 运行期：CASH 在本窗口的 UB 改进量、LB 改进量、SPB 上一轮是否给出可行解、SPB 上一轮是否严格改善。

在 `HybridAdaptive` 的退避之上，把判据从「连续 barren 若干轮则加倍窗口」升级为「预测本轮是否有正收益，
否则跳过」。判据的拟合与评估使用已有数据：`runs/iter2_debug50/`（600 次）与 `runs/full2cfg/`（1116 次）；
**这是离线分析与方法设计，不需要新的实验预算**。

### A3.3 可证伪判据

- **H3a**：用可解释特征预测「本轮交接是否严格改善 UB」显著优于随机交接。
- **H3b**：选择性交接在证明数不下降的前提下，减少总交接次数（降低代价）。
- **H3c**：特征在 `full2cfg` 上是否具有跨实例族稳定性（按 MSE 实例族分层报告，避免只在调试集上过拟合）。

### A3.4 定位

这是**增量式**贡献，适合作为论文的一节而非主线。它的主要价值是把「112 亏 : 0 赚」这一负结果
转化为方法本身的一部分，而不是把它留在讨论章节。

---

## 4. 三点的组合、实验矩阵与指标口径

### 4.1 配置阶梯

| 配置 | 相对前一档的增量 | 回答的问题 |
|---|---|---|
| `CASH` | 精确基线 | — |
| `HybridNatural` | 非抢占交接（已实现） | 不打断 CASH 时 SPB 有无贡献 |
| `HybridThreshold` | + A1 阈值下发与提前返回 | 交接是否应按硬化收益而非 UB 差值来组织 |
| `HybridPhase` | + A2 相位注入 | 赋值通道是否比标量 UB 通道更有价值 |
| `HybridThresholdPhase` | A1 + A2 | 两条通道是否互补 |
| `HybridSelective` | + A3 选择性触发 | 能否在不损失证明数的前提下减少交接 |

### 4.2 指标口径

主指标「是否证明最优」在本问题上天然不敏感（§0.2），建议**预先登记**一组复合口径，
并在论文中说明选择理由：

1. **证明数**（不变，保证与既有结论可比）；
2. **未证明实例上的最终 gap**：以 `final_ub / final_lb` 或与已知最优的比值度量；
3. **anytime 指标**：逐轮 UB 的改善轨迹（`events.jsonl` 已有），对「提前收紧」这一真实贡献敏感；
4. **机制指标**：新增硬化子句数、跨阈次数、`UB - LB` 区间随时间的变化。

方法学上，anytime 性能的比较方式可参考 ECDF 类做法（见文献 [7]）。

### 4.3 与开题/中期报告的对应

- A2 直接对应开题报告研究内容（2）「双向交互机制」中缺失的反方向，可写入中期报告 4.5.2 节
  作为「已尝试的改进方法」的设计依据。
- A1 的机制论证（§0.3）可写入中期报告第 4 章，作为对混合失效原因的定位结果。
- 中期报告现有正文声称「尚未完成正式对比实验」，与 `runs/` 中的实际批次不一致，
  修订时需按实际产出更新（此项超出本文范围，另行处理）。

---

## 5. 相关文献（2024-2026，已核对摘要）

| # | 文献 | 与本文的关系 |
|---|---|---|
| [1] | Anytime Cooperative Implicit Hitting Set Solving, arXiv:2501.07896 (2025-01) | 最接近的「上下界组件互相共享、anytime 缩gap」的并行协同架构，是 B 线的直接参照 |
| [2] | Enhancing Local Search for MaxSAT with Deep Differentiation Clause Weighting (DeepDist), arXiv:2512.05619 (2025-12) | DeepDist + TT-Open-WBO-Inc 的混合已超过 MSE24 冠军 SPB-MaxSAT-c-Band/FPS，即本项目 SPB 组件所属族；构成基线更新的必要性 |
| [3] | Integer Linear Programming Preprocessing for Maximum Satisfiability, arXiv:2506.06216 (2025-06) | MaxSAT 求解器中的 ILP 组合策略；与 CASH 的 SCIP 分量直接相关 |
| [4] | Introducing Clause Cuts: Strong No-Good Cuts for MaxSAT in MILP, arXiv:2509.21687 (2025-09) | 用 SAT oracle/CDCL 学习子句强化 MILP 表述，可支撑 B 线中 SCIP 侧的改进 |
| [5] | Efficient and Reliable Hitting-Set Computations for the IHS Approach, arXiv:2508.07015 (2025-08) | 用 PB 推理与局部搜索做碰集计算，并给出可验证性讨论；与「LS 进入核心引导框架」相关 |
| [6] | Certified Branch-and-Bound MaxSAT Solving (Extended Version), arXiv:2511.10273 (2025-11) | 精确 MaxSAT 的证明日志；若做证书化混合需以此为基准 |
| [7] | Better Understandings and Configurations in MaxSAT Local Search Solvers via Anytime Performance Analysis, arXiv:2403.06568 (2024-03) | anytime 性能的评估方法学（ECDF），支撑 §4.2 的指标口径 |
| [8] | Rethinking the Soft Conflict Pseudo Boolean Constraint on MaxSAT Local Search Solvers, arXiv:2401.10589 (2024-01) | 软冲突结构对 LS 加权的影响，是 B 线「核心引导加权」的依据 |
| [9] | ParLS-PBO: A Parallel Local Search Solver for Pseudo Boolean Optimization, arXiv:2407.21729 (2024-07) | 并行局部搜索的工程参照 |

补充事实：**MaxSAT Evaluation 2025 未举办**（官方公告为「we regret to inform you that the MaxSAT 2025
evaluation will not be held this year」，期望 2026 年恢复），因此 MSE24 仍是当前最新的官方基准口径。

---

## 6. 状态与下一步

- 本文记录的 A1/A2/A3 **均未实现**，服务器上不存在对应代码；`HybridNatural` 与 `HybridAdaptive`
  已实现，见 commit `e18f1c1`。
- 实现前必须先补的正确性测试（沿用 `experiment.md` §10 的 Gate 结构）：
  1. 阈值访问器在 `goal_gcd` / `fixed_goalval` / `harden_goalval` 各分支下返回同一内部单位；
  2. 硬化子句计数与实际 `harden_soft_cls()` 行为一致；
  3. 相位注入不影响 `LB`、`UB` 与最终最优性判定（注入前后对同一实例、同一种子给出相同判定的回归）。
- 建议顺序：A1 的只读访问器与日志字段 → A1 的阈值下发 → A2 的相位注入 → A3 的离线分析。
  A3 不占用实验预算，可与其他工作并行。
- 本文与 `plan.md`、`experiment.md` 的关系：本文只新增候选设计与验证判据，不修改既有的调度协议、
  正确性约束与实验验收规则。

---

## 附：本文数据来源

    runs/iter2_debug50/          50 实例 × 4 配置 × 3 种子，含 summary.tsv 与逐轮 events.jsonl
    runs/iter1_debug50/          同上，迭代 1 版本
    runs/iter1_losses/           抢占式损失的 9 个实例，配对验证
    runs/full2cfg/               558 实例 × 2 配置（CASH / 抢占式 Hybrid）
    runs/full23w_1seed/          运行中：558 实例 × 2 配置 × 1 种子

代码位置引用均指向本仓库当前 `server/code` 分支（HEAD = `e18f1c1`）的实际文件。