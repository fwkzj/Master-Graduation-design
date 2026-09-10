# HybridMaxSAT 实验要求与输入输出指导

本文档是实验执行的**操作规范**：规定跑哪些配置、用什么种子与预算、如何抽样、如何调用运行器、产出什么文件与字段。算法与正确性约束以 `agent.md` 为准，实施与阶段划分以 `plan.md` 为准。本文档不与它们冲突，只补充实验执行层面的输入输出契约。

## 1. 实验目标

对加权部分 MaxSAT（WPMS）实例，比较四类配置在 600 秒端到端预算下的表现，核心指标是「由 CASH 精确推理证明 `LB = UB`（最优）」的次数。独立 SPB 仅作基线，不标为证明最优。

## 2. 配置矩阵

| 配置 | 行为 | 说明 |
|---|---|---|
| `CASH` | CASH 的 15 秒窗口连续运行 600 秒，从不调用 SPB | 精确基线，即混合协议去掉 SPB 一轮 |
| `SPB` | 独立 SPB 连续运行 600 秒 | 局部搜索基线，只回数值 UB |
| `Hybrid` | 15s CASH / 3s SPB 暂停-恢复协同 | 正式混合配置，用 CASH 推理值初始化 SPB |
| `Hybrid-NoInference` | 同 `Hybrid`，但 SPB 每轮全部随机初始化 | 消融：不把 CASH 推理值用于 SPB 初始解 |

四配置构成**消融阶梯**而非四个互不相关的求解器：`CASH` 与 `Hybrid` 只差 SPB 轮次，因此
`Hybrid` 与 `CASH` 之差即 SPB 组件的边际贡献。为此 `CASH` 与 `Hybrid` 的 SCIP 组件使用
相同的时限（见 §4），否则 `CASH` 会在前几百秒里一直停留在 SCIP，其 CDCL 主循环——本工作
真正研究的部件——根本不会运行。

不增加「重置 SPB 自适应子句权重」的消融。每个实例、每种配置、每次重复使用同一实例派生种子。

## 3. 种子与随机性

- 调试集抽样种子：`20260909`。
- 每个实例的相对路径与全局种子共同派生该实例的随机序列。
- 正式实验每实例每配置重复三次，重复种子为 `20260909`、`20260910`、`20260911`。
- 同一实例、同一重复内的 `SPB`、`Hybrid`、`Hybrid-NoInference` 使用相同的实例派生种子，保证可比。

## 4. 时间预算与停止规则

- 每实例端到端预算 600 秒，计入 CASH、SPB 与交接开销。
- 600 秒到达后不启动下一模块，当前模块完整结束后停止：
  - 当前模块为 SPB：完整跑完该 3 秒窗口、接收其 UB 一次后结束。
  - 当前模块为 CASH：完成该 15 秒窗口后结束。
- 调试 50 实例同样使用 600 秒与完整协议。
- CASH 一旦证明最优，立即结束该实例，不再启动 SPB。这条对 CASH 的 ILP 分量（SCIP）同样
  成立：SCIP 在自身 15 秒时限内证出最优时，实例同样立即结束，`exit_reason` 记为
  `optimum_scip` 以区别于 CDCL 推出的 `optimum`（见 §8.3）。

### 4.1 窗口的四道闸门

一个「15 秒 CASH 窗口」要真正成立，CASH 必须在 15 秒内回到它的调度点。实测发现有四处
会各自吞掉整个预算、使调度点不再被访问（详见 `docs/experiments/` 的过程记录）：

1. **SCIP 首调用**：CASH 在进入自己的 CDCL 循环之前，先把实例交给 SCIP，且该调用同步执行、
   默认**无时限**。大实例上 SCIP 连预处理都跑不完，于是 600 秒全耗在这里，混合协议一次都
   不会被触发。因此 `opt_scip_cpu` 被设为 CASH 窗口长度（15 秒），四种配置一致。
2. **SCIP 重试抬时限**：SCIP 打满 15 秒且上下界 gap<10% 时，CASH 的重试启发式会把
   `opt_scip_cpu` 加 `opt_scip_cpu_add`（600 秒），等于把窗口悄悄改成 615 秒、让 SCIP 重新
   吞掉预算。嵌入模式（`opt_embedded_runner`）已禁用这条重试，SCIP 始终只拿 15 秒。
3. **单次 SAT 调用**：由 `cash_deadline_reached` 终结器在窗口边界让 CaDiCaL 返回。
4. **进程级兜底**：调度脚本用 `timeout` 包住每次运行（预算 +15 秒，再 30 秒后 SIGKILL），
   运行器自身另有 budget+3 秒的看门狗线程保证一定写出记录。

调度脚本把每轮的 `cash_deadline_polls`（CaDiCaL 询问终结器的次数）写入运行记录：一个跑了
几百秒而该值为 0 的运行，意味着终结器根本没被问到，与「一次 SAT 调用超窗」在轮次数上无法
区分，必须靠这个计数器分辨。

### 4.2 预算与 CPU 时间

窗口与预算一律按**墙钟**计时，不使用 CASH 回调传入的 CPU 时间：CASH 的 SCIP 组件跑在第二个
线程上，进程 CPU 时间比墙钟快得多，以 CPU 时间做窗口会让协议悄悄缩水。端到端预算也据此
与 SPB 自身的 `Settings::cutoff_time`（墙钟）对齐，四种配置才可比。

CASH 自带的 `-cpu-lim` 未被使用：它把 `RLIMIT_CPU` 设在预算的**四分之一**处，并指望求解
循环吸收第一次信号，该吸收由一个一次性的 `first_time` 标志保护，实测在大实例上不会被吸收，
CASH 会在约 1/4 预算处停下。

## 5. 数据集与抽样

- 数据只读：`/data/dataset/Maxsat/Complete/`，禁止修改其中任何文件。
- 调试阶段：从 `MSE23W` 均匀抽取 50 个 `.wcnf`。解析失败的候选记录原因、跳过并从剩余文件补抽，直到凑满 50 个可运行实例；最终清单须纳入版本控制。
- 正式阶段：`MSE23W` 与 `MSE24W` 的全部加权实例，跑上述四配置 × 三次重复。
- 调试批次任何运行出现崩溃、解析错误、非法界值或日志不一致，立即停止整个批次；修复后从头重跑。

## 6. 验收与结果口径

- 主要结果：「3 次中由 CASH 证明最优的次数」。
- 每次重复单独保留端到端耗时；记录最终 LB 与 UB 作为解释性数据。
- 调试报告给出 50 实例状态表，逐项标明四配置是否正常完成、CASH 是否证明最优。
- 正式报告按配置统计三次成功数与时间，不只汇报最好一次。
- 只有 CASH 的精确推理使 `LB = UB` 时才计为证明最优；外部 UB 永不单独宣布最优。

## 7. 目录约定

```text
HybridMaxSAT/
  configs/       # 调试/正式实验配置（实例清单、种子、预算、并行度）
  manifests/     # 实例清单与抽样记录（纳入版本控制）
  scripts/       # 构建、运行、汇总脚本
  runs/          # 大型日志与结果（不纳入版本控制）
```

## 8. 运行器与输入输出

### 8.1 输入

运行器接收一个 WCNF 文件路径与一个输出 JSONL 路径：

```bash
hybridmaxsat --config CASH|SPB|Hybrid|HybridNoInference --seed N --budget SECONDS \
             [--cash-window SECONDS] [--spb-window SECONDS] [--scip-cpu SECONDS] \
             [--instance-id RELATIVE_PATH] <input.wcnf> <events.jsonl>
```

默认值为 `--config Hybrid --seed 20260909 --budget 600 --cash-window 15 --spb-window 3`；
`--scip-cpu 0`（默认）表示由 CASH 窗口长度推导。窗口与 SCIP 时限可由调度脚本统一改写，
但正式批次固定使用 15/3/15。

WCNF 从只读数据路径读取；实例相对路径（如 `MSE23W/xxx.wcnf`）作为 `instance` 字段写入事件，用于跨配置、跨重复对齐。

### 8.2 输出：逐轮事件（JSONL，每行一个对象）

| 字段 | 类型 | 含义 |
|---|---|---|
| `event` | string | 固定 `spb_round` |
| `instance` | string | 实例相对路径 |
| `round` | int | 轮次（从 1 起） |
| `cash_seconds_before_spb` | number | 本轮调用 SPB 前的协议墙钟耗时（秒），从 CASH 首次到达调度点算起 |
| `propagated_original_variables` | int | 导出的传播推理变量数量 |
| `cash_ub_before_spb` | number | 调用 SPB 前的 CASH UB（`-1` 表示未知/∞） |
| `spb_ub` | number | SPB 本轮最佳数值 UB（`-1` 表示本轮无可行解） |
| `cash_ub_after_spb` | number | CASH 处理后的 UB（未改善则等于轮前 UB） |
| `cash_bound_result` | string | `accepted` / `not_improved` / `invalid_negative` / `invalid_unit_conversion` / `invalid_below_lb` |

### 8.3 输出：运行完成行（单行 JSON）

| 字段 | 含义 |
|---|---|
| `event` | 固定 `run_complete` |
| `instance` | 实例相对路径 |
| `config` | `CASH` / `SPB` / `Hybrid` / `HybridNoInference` |
| `seed` | 本次重复使用的种子 |
| `budget_seconds` | 端到端预算 |
| `start_epoch` / `end_epoch` | 起止 UNIX 时间（秒） |
| `wall_seconds` | 端到端耗时（秒） |
| `cash_proved_optimal` | CASH 是否证明最优（`true`/`false`）；只有该字段为 `true` 才算「证明最优」 |
| `scip_proved_optimal` | 该最优证明是否来自 CASH 的 ILP 分量（SCIP），而非其 CDCL 搜索；为 `true` 时 `cash_proved_optimal` 也必为 `true` |
| `final_lb` | 最终下界（CASH 内部目标单位） |
| `final_ub` | 最终上界（CASH 内部目标单位） |
| `final_incumbent` | 最终 incumbent（`best_goalvalue`） |
| `spb_certificate_ub` | SPB 私有证书 UB，无证书则为 `null` |
| `cash_deadline_polls` | CaDiCaL 询问窗口终结器的次数 |
| `cash_window_seconds` / `spb_window_seconds` / `scip_seconds` | 本次实际生效的三项时限 |
| `exit_reason` | 见下表 |

界值一律为十进制字符串；`n/a` 表示**该配置不产生这个值**（SPB 不推导下界；被看门狗
截断的运行读不到界值），与任何数值界都不同。它不写作 `+oo`：作为下界 `+oo` 语义是反的。

| `exit_reason` | 含义 |
|---|---|
| `optimum` | CASH 的 CDCL 搜索证明 `LB = UB`，正常提前结束 |
| `optimum_scip` | CASH 的 ILP 分量（SCIP）在自身时限内证出最优，混合协议尚未开始交接 |
| `budget` | 到达端到端预算，由求解器自身在调度点正常结束 |
| `budget_no_bounds` | 看门狗在预算+3 秒触发，界值读取失败 |
| `budget_signal` | 进程收到 SIGTERM（调度脚本的 `timeout`），由信号处理器写出最小记录 |
| `completed` | 预留：求解器自行结束且未命中以上情形 |

### 8.4 退出码与错误

- `0`：正常结束（含达到预算、未证明最优的结束）。
- `1`：异常（解析失败、无法写输出等），错误信息写入 stderr。
- `2`：命令行参数数量错误。
- `124`：**由调度脚本的 `timeout` 触发**。求解器自身在预算+3 秒内就会写出记录并退出，
  因此出现 124 说明兜底机制被用到，是**发现**而非例行情况，调试批次据此判失败。

### 8.5 运行级必记字段（正式批次在事件之上补充）

每实例、每配置、每种子的运行在 `run_complete` 之上再落盘：

```text
runs/<批次>/<配置>/<种子>/<实例名>/
  cmd.txt        # 完整命令行（含 timeout 包装），可直接复现
  stdout.log     # 求解器 stdout，内含 run_complete 行
  stderr.log     # 错误信息
  events.jsonl   # 逐轮事件
  status.json    # 解析后的记录、退出码、耗时、轮次、问题列表
runs/<批次>/meta/meta.json      # git commit、二进制 sha1、清单 sha1、uname、核数、调度参数
runs/<批次>/experiment.log      # 每行一次运行的追加日志
runs/<批次>/summary.tsv|.md     # 批次汇总
```

`summary.tsv` 中 `with_ub` 是有可行解的运行数，`productive_rounds` 是**产出可行解**的 SPB
轮数——轮次多但该值为 0，说明交接发生了但 SPB 没有贡献，与「交接根本没发生」是两种不同的
结论，必须分开统计。

## 9. 并行执行

服务器 S122 有 256 核。实验以**并行度 32** 调度：同一时刻最多 32 个实例在跑（每个实例单线程，不跨核并行）。由运行/汇总脚本按 `configs/` 中的实例清单与种子展开任务、限流到 32、并把每个任务的 stdout/stderr 与 JSONL 落到 `runs/` 下独立目录；任务失败不拖垮批次，但进入 `runs/` 前应先通过单实例冒烟与 Parser Gate。

## 10. 前置条件（Gate）

在启动任何数据集实验前，以下必须在服务器通过：

1. SPB 标准 WCNF 解析（Parser Gate）：`p wcnf` 头、`top` 与 `h` 硬子句、空软/空硬子句、`verify_solution` 成本语义。
2. SPB 持久接口：单对象跨轮保留自适应子句权重、按新赋值重建搜索状态。
3. 外部 UB 桥接：严格改善 / 相等变差 / 故意过小 UB、含固定代价实例、权重有公约数实例、触发 CASH 硬化的实例，每例检查原目标、内部目标单位、LB、UB 与最终模型一致。
4. `hybridmaxsat` 运行器在单个小实例上冒烟通过。

### 10.1 完成情况

| 项 | 状态 | 证据 |
|---|---|---|
| 1 解析 | 已通过 | `SPBMAXSAT2-master/SPBMAXSAT2-master/tests/` 回归测试 |
| 2 持久接口 | 已通过 | 同上 |
| 3 外部 UB 桥接 | 已通过（Stage 0） | `tests/instances/fixed_cost.wcnf`（任意赋值代价恒为 10）、`tests/instances/weight_gcd.wcnf`（软子句权重的公约数为 3，最优 6）；两例在四配置上均给出预期最优，见 `runs/stage0/` |
| 4 运行器冒烟 | 已通过（Stage 0） | `manifests/smoke.txt` 五实例 × 四配置 × 三重复 = 60 次运行全部 `exit 0`、无 `problems` |

Stage 0 全部界值复核（四配置 × 三重复全部一致）：

| 实例 | 手算最优 | 结果 |
|---|---|---|
| `fixed_cost.wcnf` | 10 | 10 |
| `weight_gcd.wcnf` | 6 | 6 |
| `standard_weighted.wcnf` | 3 | 3 |
| `hard_keyword.wcnf` | 7 | 7 |
| `persistent_interface.wcnf` | 3 | 3 |

### 10.2 已知限制

- **SPB 在超大实例上给不出可行解。** 例如 `MSE23W/hs-timetabling_wt-BrazilInstance7.xml.wcnf`
  （55532 变量）上，独立 SPB 跑满 40 秒也没有可行解，混合配置把窗口放宽到 20 秒同样没有。
  这类实例上的混合轮次必然「有轮次、无贡献」，属于 SPB 本身的能力边界，不是协调器缺陷。
- **CASH 传递的传播状态只覆盖很小一部分变量。** 同一实例上每轮 `propagated_original_variables`
  约 2600 / 55532，其余变量由 SPB 自行随机补全；这正是 `Hybrid` 与 `Hybrid-NoInference`
  差异的所在，也是短窗口内在超大实例上难以取得可行解的原因。
- **SCIP 提前解掉实例会掩盖混合协议。** 在 SCIP 能在 15 秒内证出最优的实例上（如
  `Security-CriticalCyber-PhysicalComponents`、`frb25-13-1`），CASH/Hybrid/HybridNoInference
  三配置都在首窗口前结束，混合协议根本没有机会交接，四配置的差异在这类实例上无从体现。
  这类运行以 `optimum_scip` 标注、在 `summary.tsv` 的 `via_scip` 列单独计数，不能与 CDCL 推出的
  `optimum` 混为一谈；正式分析应按「SCIP 可解」与「SCIP 不可解」分层。
