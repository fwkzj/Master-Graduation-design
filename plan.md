# CASHWMaxSAT + SPBMAXSAT2：WPMS 混合实验计划

## 1. 实验目标与前提

目标是在不重写 CASHWMaxSAT 的精确推理或 SPBMAXSAT2 的局部搜索策略的前提下，逐步检验：局部搜索给出的、**独立验证过的**候选解是否能改善精确求解的起始或阶段性上界，以及保守的时间片调度是否有可复现收益。

当前最重要的结论是：SPBMAXSAT2 的 WCNF 解析器尚不能可靠处理普通标准 WCNF。因而“SPB 输出可直接提供 UB”不是既有事实。所有性能实验必须等待 Gate 0 通过。

## 2. 初始化与已验证命令

```bash
export PROJECT=/home/fwkzj/HybridAlgorithm
export CASH_ROOT="$PROJECT/CASHWMaxSAT-DisjCad-S6"
export SPB_ROOT="$PROJECT/SPBMAXSAT2-master/SPBMAXSAT2-master"
export SAMPLE="$CASH_ROOT/code/uwrmaxsat/Examples/satellite02ac.wcsp.wcnf"
mkdir -p "$PROJECT/build" "$PROJECT/work/instances" "$PROJECT/runs"
```

构建 SPB：

```bash
cmake -S "$SPB_ROOT" -B "$PROJECT/build/spbmaxsat" -DCMAKE_BUILD_TYPE=Release
cmake --build "$PROJECT/build/spbmaxsat" -j 4
```

产物为 `build/spbmaxsat/solver` 与 `build/spbmaxsat/libspbmaxsat.a`。复制并运行 CASH：

```bash
install -m 700 "$CASH_ROOT/bin/cashwmaxsat-disjcad" "$PROJECT/build/cashwmaxsat-disjcad"
"$PROJECT/build/cashwmaxsat-disjcad" \
  -cpu-lim=10 -mem-lim=1024 -v0 -no-bin -no-sat -m -bm -no-scip \
  "$SAMPLE" > "$PROJECT/runs/cash-smoke.stdout" \
  2> "$PROJECT/runs/cash-smoke.stderr"
```

这条 CASH 命令已在样例上输出 `o 1611` 和 `s OPTIMUM FOUND`。正式脚本必须解析最后的 `o`、`s` 与 `v` 行，不能只看退出码。

## 3. Gate 0：输入语义与候选验证

### 3.1 修复 SPB 解析器

修复 `SPBMAXSAT2/.../src/BasicStruct/instance.cpp` 中的 `Instance::build_instance()`：

1. 第一遍只读取并严格校验 WCNF 头 `p wcnf <nvars> <nclauses> <top>`；头部字段必须是唯一的内存分配依据。
2. 依据头部创建全部数组，第二遍读入恰好声明数量的子句，并检查文字范围与终止 `0`。
3. 数值权重等于 `top` 的子句为硬子句；若支持扩展 WCNF，`h` 子句在内部也规范化为 `top`。
4. 保留所有原始软权重，使用 64 位整数累计，并对异常格式返回明确错误。

不要在此阶段修改 LSworker、动态加权、变量选择、交叉或解池逻辑。

### 3.2 新建独立验证器

验证器不得只调用 SPB 的 `verify_solution()`。输入为原始 WCNF 和完整 0/1 模型，输出为：

```text
feasible, cost, violated_hard_clause, model_hash, parser_version
```

它必须检查模型尺寸与取值，检查每个硬子句，按原始软权重重算违反代价，检测整数溢出。`IncumbentStore` 只从此验证器接收 `feasible=true` 的候选。

### 3.3 Gate 0 验收

| 检查 | 通过标准 |
|---|---|
| 手工小实例 | 覆盖零/非零代价、不同权重、硬不可满足、重复和互补文字 |
| 标准 WCNF | SPB 和独立验证器对变量数、子句数、top、硬/软分类一致 |
| 扩展格式 | 未支持时明确拒绝；支持 `h` 后有专项回归测试 |
| CASH 交叉验证 | 小实例与自带样例上，SPB 返回模型的独立成本不低于 CASH 的已证明最优值 |
| 负例 | 篡改模型、截断模型、伪造成本、违反硬子句均被拒绝 |

`satellite02ac.wcsp.wcnf` 上“CASH=1611、SPB=0”的差异必须消失。否则停止，不能开始数据集实验。

## 4. 数据选择和准备

公共基准只读位于：

```text
/data/dataset/Maxsat/Complete
/data/dataset/Maxsat/Incomplete
```

建立三个互斥清单，并在 manifest 中保存路径、大小、SHA-256、年份、加权类型、实例族和格式：

1. 调试集：8–20 个小型传统 WCNF，用于解析和冒烟测试。
2. 开发集：每个选定实例族 10–30 个，用于固定参数和阈值。
3. 测试集：按实例族、年份和规模与开发集分离，仅在配置冻结后运行。

压缩输入须复制到本运行的私有目录：

```bash
gzip -cd "$SOURCE_WCNF_GZ" > "$PROJECT/work/instances/$INSTANCE_ID.wcnf"
```

初期仅接受传统 `p wcnf` 与数值 `top`。文件扩展名相同不代表格式相同；含 `h` 子句或无传统头的 MSE 扩展格式，必须等扩展解析器通过专项测试再纳入。

## 5. 单求解器基线

### B0：CASH 精确基线

```bash
"$PROJECT/build/cashwmaxsat-disjcad" \
  -cpu-lim=600 -mem-lim=8192 -v0 -no-bin -no-sat -m -bm -no-scip \
  "$INSTANCE" > "$RUN_DIR/cash.stdout" 2> "$RUN_DIR/cash.stderr"
```

仅 `s OPTIMUM FOUND` 记为证明最优。`UNKNOWN` 必须保留最后的成本、时间和状态，不能写作最优或失败。保存完整二进制 `v` 模型并由独立验证器复算。

### B1：SPB 局部搜索基线

正式基线使用静态库驱动，而不是只读 `solver` 的 `bestcost`：

```cpp
Settings cfg;
cfg.cutoff_time = 60;
cfg.solution_pool_size = 1;
util::set_seed(seed);
spbmaxsat::LocalSearchSolver ls(instance_path, cfg);
Solution candidate = ls.solve();
```

每个实例运行多个独立种子，独立验证每个模型，报告可行率、最好值、中位数和四分位数。命令行 `solver` 可保留为调试工具。

## 6. 融合层实验

### H0：验证式热启动

SPB 在固定小预算内产生模型；独立验证器重算后，`IncumbentStore` 只接受严格更优候选。CASH 当前没有已验证的在线 UB API。`-goal=<verified_ub>` 只是候选目标限制，必须先在小实例上证明其输出和证明语义正确；若不能安全使用，H0 只记录 incumbent，不改变 CASH 的输入。

### H1：串行共享

使用保守流程：

```text
SPB 时间片 → 独立验证 → 记录/提交 UB → CASH 新时间片 → 解析状态 → 下一轮
```

若 CASH 必须重启才能读取新边界，重启次数和时间必须记入总预算与对比结果。禁止向运行中的 CASH 进程写入模型或成本。

### H2：精确部分赋值驱动局部搜索

精确侧若能在受测试的安全点导出原变量的部分赋值，应将已证明的变量取值投影为 SPB 的 1-based 初始向量；没有被证明的变量固定写为 `-1`。随后调用 `LocalSearchSolver::improve()`，由 SPB 补全和改进。

每个传递的取值必须记录其来源、原变量编号、推理阶段和投影规则。内部松弛变量、辅助变量、临时相位和仅具启发意义的猜测不得传递。若当前 CASH 命令行不能导出这种受证明的中间状态，本层保持未实现，而不是用最终模型或猜测冒充“子局部解”。

### H3：确定性自适应调度

第一版只用已观测数据：剩余资源、最近是否有经验证 UB 改善、SPB 连续无改善次数、CASH 完成状态和累计重启成本。建议规则：

| 条件 | 动作 |
|---|---|
| 无可行 UB | 给 SPB 一个短热启动片 |
| 最近 SPB 片改善 UB | 允许一次额外 SPB 片，受总预算上限约束 |
| SPB 连续 `k` 片无改善 | 转向 CASH 或停止 SPB |
| CASH 已证明最优 | 立即结束 |
| 资源耗尽 | 输出 `UnknownWithinBudget` 和经验证 UB |

当前不得声称使用核心大小、LB 增长率或在线双向通信；这些状态没有被现有 CASH 命令行公开。新增适配器并通过接口测试后，才可讨论下一层实验。

## 7. 对比、日志和判定

在相同总 CPU、墙钟和内存限制下比较：

| ID | 方法 | 作用 |
|---|---|---|
| B0 | CASH 单独运行 | 精确基线 |
| B1 | SPB 单独运行，多种子 | 局部搜索基线 |
| H0 | 验证式热启动 | 初始 UB 的贡献 |
| H1 | 固定时间片串行共享 | 持续 UB 共享的贡献 |
| H2 | 精确部分赋值初始化 SPB | 精确→局部搜索协同的贡献 |
| H3 | 自适应时间片 | 调度的贡献 |

每个运行目录保存 `command.txt`、stdout/stderr、模型、`events.jsonl` 和 `result.json`。后者至少包含实例与哈希、代码/解析器版本、配置、种子、资源限制、每个时间片、claimed/verified cost、接受或拒绝原因、CASH 状态、最终 UB、证明标记和峰值内存。

主要指标：证明最优数、首次可行解时间、首次经验证 UB 时间、最终经验证 UB、合法 LB 存在时的间隙、总时间和峰值内存。按实例族报告成功和失败，并做配对比较；不得只展示提升实例。

## 8. 实施顺序与停止规则

1. 写独立验证器和小实例测试。
2. 修复 SPB WCNF 解析并通过 Gate 0。
3. 实现输出完整模型、种子和结构化日志的 SPB 库驱动，完成 B0/B1。
4. 在小实例验证 CASH `-goal` 的语义；不安全则放弃此注入路径。
5. 实现 H0、H1 和 H2，逐层做消融。
6. 只有实际 API 支持且测试充分后，才扩展到 H3 的调度交互。

出现以下任一情况必须停止当前批次、修复并重跑：候选未通过独立验证；SPB 与独立成本不一致；CASH 输出无法解析；格式未列入支持范围；种子/配置缺失；或重启成本未计入。未通过 Gate 0 的输出只能作为缺陷复现记录，不能用于论文实验图表或性能结论。
