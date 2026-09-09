# WPMS 混合求解实验：协作与执行指南

## 本地禁止运行

服务器 S122 的 `/home/fwkzj/HybridAlgorithm` 是所有构建、测试、解析和实验的唯一执行环境。服务器连接恢复前，本地只能编写、审阅、提交和推送文件；禁止本地构建、编译、运行测试、解析基准、运行 CASH/SPB 或执行任何实验。当前本地 Git 镜像位于 `D:\硕士毕设\.upload_stage_20260909\repo`，其提交将在服务器恢复后同步回项目目录。

本地文件先集中完成一个可审查阶段，再统一提交和推送 GitHub；不要为每条小型记录单独上传。

## 变换正确性

CASH 对软子句进行松弛和 PB/SAT 变换；SPB 的原始 WCNF UB 不能直接写入 CASH 内部 LB/UB 字段，也不能由协同层自行硬化子句。SPB 可以只对 CASH 输出数值 UB，但在输出前必须私有保存完整模型并按原始 WCNF 核对硬子句满足和目标值一致。外部 UB 只能在 CASH 变换完成后的安全点，经唯一且经服务器测试的单位转换与 CASH 自身界处理入口接收；仅 CASH 的 `LB = UB` 可证明最优。

## 目标与角色

本项目研究加权部分 MaxSAT（WPMS）的混合求解：**CASHWMaxSAT-DisjCad-S6** 是精确求解器，负责精确结果、下界和最优性证明；**SPBMAXSAT2** 是局部搜索求解器，负责提供候选赋值和候选上界。研究对象是经过验证的上界共享与调度策略，而不是修改两套基础算法。

原始 WCNF 是唯一语义来源。若硬子句集合为 `H`、软子句集合为 `S`、软权重为 `w(C)`，完整赋值 `a` 的真实代价是：

```text
cost(a) = Σ w(C)，其中 C ∈ S 且 a 不满足 C
```

局部搜索的动态权重、罚函数和内部评分不等于 `cost(a)`；它们不得作为 UB、LB、硬化条件或最优性依据。

## 已核实的环境与接口

| 组件 | 位置 | 已核实事实 |
|---|---|---|
| 项目根目录 | `/home/fwkzj/HybridAlgorithm` | 放置融合层、配置、日志和实验结果 |
| CASH | `CASHWMaxSAT-DisjCad-S6` | 单次命令行精确求解；输入 WCNF；可输出 `o`、`s`、`v` 行 |
| SPB | `SPBMAXSAT2-master/SPBMAXSAT2-master` | CMake 构建 `solver` 与静态库 `spbmaxsat`；同步单线程 C++ 封装 |
| 公共数据 | `/data/dataset/Maxsat` | 可读，约 7,891 个 WCNF/WCNF.GZ 算例、约 549 GB |

CASH 的 `bin/cashwmaxsat-disjcad` 当前没有执行位。实验脚本应复制到 `build/` 后用 `install -m 700` 设置权限，避免修改原文件。CASH 的已验证选项包括 `-m`、`-cpu-lim=<seconds>`、`-mem-lim=<MB>`、`-bm`、`-no-sat`、`-no-scip` 与 `-goal=<value>`。输出含义：

```text
o <cost>              # 当前或最终目标值
s OPTIMUM FOUND       # 已证明最优
s UNSATISFIABLE       # 原问题不可满足
s UNKNOWN             # 未完成证明
v <0/1 string>        # 使用 -bm 时的模型
```

在仓库样例 `satellite02ac.wcsp.wcnf` 上，CASH 在 10 秒 CPU 限制下输出 `o 1611` 和 `s OPTIMUM FOUND`。这只是命令链路的烟雾测试，不是性能结论。

SPB 的命令行为 `./solver <input.wcnf> [options]`，支持 `-cutoff`、`-pool_size`、`-rdprob`、`-bms_num`、`-rwprob`、`-hard_sp`、`-soft_sp`、`-h_inc`、`-s_inc`。命令行只打印 `c bestcost`，不输出完整赋值，因此只能作为局部搜索基线。融合层必须链接库并使用：

```cpp
Settings cfg;
cfg.cutoff_time = 10;
cfg.solution_pool_size = 1;
spbmaxsat::LocalSearchSolver ls(instance_path, cfg);
Solution result = ls.solve();
// result.feasible, result.cost, result.assignment
```

`improve(initial)` 接受长度为 `num_vars + 1` 的 1-based 向量：第 0 位不用，元素为 `0`、`1` 或 `-1`；`-1` 由 SPB 随机补全。其随机数和计时器为进程级状态，禁止在同一进程并发运行多个 SPB 实例。

## Gate 0：当前解析器阻塞

当前 SPB 的 `src/BasicStruct/instance.cpp` 不能直接用于标准公共 WCNF：

1. `build_instance()` 在读取 `p wcnf <nvars> <nclauses> <top>` 前就分配内存，依赖非标准注释字段。
2. `h ... 0` 硬子句被赋予 `LLONG_MAX`，而后续又用 `weight == top_clause_weight` 判断硬子句，语义不一致。
3. 在同一仓库样例上，CASH 证明最优值为 1611，而 SPB 命令行报告 `bestcost 0`。

在修复与验证前，SPB 的成本和模型都不得作为实验结果或共享 UB。修复必须先读取、校验头部再分配内存；数值权重为 `top` 的子句和 `h` 子句均须正确归为硬子句，并保留原始软权重。修改只限输入解析和语义验证，不得改变局部搜索策略。

## 正确性契约

1. 每个候选模型先检查长度、变量取值、全部硬子句，再按原始软权重独立重算代价。
2. `IncumbentStore` 仅接受严格改善的、已验证候选；保存模型、成本、来源、种子与时间戳。
3. CASH 是唯一可维护 LB、处理 UNSAT 核、更新精确约束和宣布最优的组件。
4. 到达资源限制只能返回 `UnknownWithinBudget` 和最后的经验证 UB；SPB 不能单独宣布最优。
5. 解析、超时、取消或验证异常发生时，拒绝本次交互并保留上一次已验证状态。

## 分级实施范围

| 层级 | 内容 | 进入条件 |
|---|---|---|
| L0 | SPB 产生模型，独立验证后记录初始 UB；CASH 独立运行 | Parser Gate 通过 |
| L1 | 串行时间片：SPB 候选验证后，在 CASH 的合法重启边界应用已验证边界 | L0 正确，`-goal` 语义已在小实例测试 |
| L2 | 精确侧将已证明的原变量部分赋值投影给 SPB `improve()`；未定变量写为 `-1` | 精确侧适配器能给出可审计的来源与变量映射 |
| L3 | 用经验证 UB 改善、精确种子效果和剩余预算选择下一时间片 | L1、L2 分别完成消融 |

在最小化违规软权重的 WCNF 定义下，局部搜索的可行模型给出 UB；若以最大化满足软权重叙述，则它给出等价 LB，必须用 `UB = total_soft_weight - LB` 转回统一的最小化语义。局部搜索不能证明子句必然满足。某子句或变量的“必须满足/固定”状态只能来自精确侧的可验证推理，并且传递给 SPB 时只能包含原变量，不能泄露内部松弛变量、辅助变量或未经证明的猜测。

当前 CASH 没有经验证的运行中 UB 注入、暂停恢复或逐步部分赋值 API，因此第一版不实现并行在线双向通信，也不声称使用核心统计调度。Exact-to-LS 方向必须先新增、测试适配器，再进入实验矩阵。

## 数据、日志与结果

公共数据只读使用。初期仅使用带传统 `p wcnf` 头和数值 `top` 的实例；`.wcnf.gz` 先解压到本次运行目录，不修改 `/data`。`h` 子句或无传统头的扩展格式，只有在单独测试支持后才纳入。

推荐目录：

```text
HybridAlgorithm/
  build/  configs/  manifests/  src/hybrid/  tests/
  work/instances/
  runs/<run-id>/{command.txt,stdout.log,stderr.log,model.txt,events.jsonl,result.json}
```

每次运行记录实例原路径和哈希、解析器版本、两个源码哈希、命令、配置、种子、CPU/墙钟/内存限制、候选验证结果、CASH 状态、经验证 UB 和失败原因。比较至少包括 CASH、SPB、L0、L1、L2；按实例族报告完成数、证明最优数、经验证 UB、时间与内存，保留失败实例，不得只报最好结果。
