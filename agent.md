# HybridMaxSAT 工作约定

## 当前工作位置

服务器 S122（`fwkzj@222.20.99.55:22`）是项目的长期运行环境，目标目录为 `/home/fwkzj/HybridAlgorithm`。截至 2026-09-10，SSH 连接已恢复正常（主机名 `HCCS-122`，256 核），批量模式免密登录可用，`build/`、`manifests/`、`runs/` 均已就位。

本地 Git 工作树为 `D:\硕士毕设`；GitHub 远端为 `git@github.com:fwkzj/Master-Graduation-design.git`（服务器侧 SSH 别名为 `git@github.com-hybridalgorithm:fwkzj/Master-Graduation-design.git`，指向同一仓库）。本地到 GitHub 的 22 端口不可用，因此本地仓库的 `origin` 指向 GitHub 的 443 端点 `ssh://git@ssh.github.com:443/fwkzj/Master-Graduation-design.git`。

开发期分工固定：**仓库只有一条长期分支 `main`；算法代码与实验侧文档只在服务器上修改，本地只写报告与文档**。服务器同时负责构建与运行。公共数据只读，路径为 `/data/dataset/Maxsat/Complete/`；不得修改其中的任何文件。

## 本地禁止运行

本地工作树只用于编写、审阅、版本控制和推送。**禁止在本地构建、编译、运行测试、解析基准、运行 CASH/SPB 或执行任何实验。**所有 Parser Gate 验证、单元测试、构建、求解和批量实验都必须在服务器 `/home/fwkzj/HybridAlgorithm` 中执行并记录。

本地文件先集中完成一个可审查阶段，再统一提交和推送 GitHub；不要为每一条小型记录单独上传。

## 单一分支协作规则

**仓库只有一条长期分支 `main`（2026-09-11 起）。** 此前并行的 `local/report`、`server/code`、`sync/implementation` 等分支已全部合并进 `main` 并删除，不再有分支间的合并操作。

### 各自改什么

| 位置 | 可以修改的范围 |
| --- | --- |
| 服务器 `/home/fwkzj/HybridAlgorithm` | 算法代码（`HybridMaxSAT/`、`CASHWMaxSAT-DisjCad-S6/`、`SPBMAXSAT2-master/`）、`scripts/`、`tests/`、`manifests/`，以及实验侧文档 `agent.md`、`plan.md`、`experiment.md`、`.planning/`、`docs/experiments/` |
| 本地 `D:\硕士毕设` | 报告与文献（`docs/reports/`、`docs/literature/`、`docs/archive/`、`artifacts/report-analysis/`） |

同一份文件只由它归属的一侧修改，两侧都提交到 `main`。本地不写代码、不跑实验，见「本地禁止运行」。

### 同步方式

- 开工前 `git pull --rebase origin main`，收工后 `git push origin main`。
- 同步粒度是「一个可审查阶段」，不要积压多日再推。
- 一切同步走 git。**禁止用 `scp` 或手工拷贝覆盖对端文件**：那会让文件一致而历史分叉，此后无法判断哪一份才是权威。
- 提交前确认工作树状态符合预期；供批次使用的提交必须是干净的，见「实验必须绑定提交」。
- 本地到 GitHub 的连接只能走 IPv6 的 443 端点，且传输较大文件时会被重置。**本地提交后若直接 `git push origin main` 失败，改走服务器中转**：

  ```bash
  # 本地 -> 服务器（同一网段，稳定）
  git push ssh://fwkzj@222.20.99.55/home/fwkzj/HybridAlgorithm main:refs/heads/import-reports
  # 服务器 -> GitHub
  cd /home/fwkzj/HybridAlgorithm
  git merge --ff-only import-reports && git push origin main && git branch -D import-reports
  ```

### 批次运行时服务器冻结

`scripts/run_batch.py` 在服务器上运行期间，**禁止在 `/home/fwkzj/HybridAlgorithm` 执行任何会改动工作树的 git 命令**（`commit`、`checkout`、`switch`、`pull`、`merge`、`rebase`、`stash`）。工作树一旦变动，正在跑的批次就无法复现，其全部结果作废。需要同步就等批次结束。

判断是否空闲：

```bash
pgrep -af run_batch.py     # 无输出表示空闲
pgrep -c  hybridmaxsat     # 应输出 0
```

只读命令（`git status`、`git log`、`git diff`）在批次运行期间仍可安全执行。

### 实验必须绑定提交

每个批次的 `meta.json` 记录 `git rev-parse HEAD` 与二进制 sha1。**工作树脏时产出的结果不采信**，因为在别处无法复现同一份源码。跑批次前必须满足：

```bash
cd /home/fwkzj/HybridAlgorithm
git status --porcelain    # 必须为空
git rev-parse HEAD        # 记入 meta.json
```

### 服务器侧必需的一次性配置

```bash
git config core.filemode false
```

CaDiCaL 的 `configure` 会把 17 个脚本的权限位从 100644 改成 100755，内容零改动，却让服务器 `git status` 常驻 17 条噪声。

### 忽略规则

`.gitignore` 覆盖仓库根目录 `/runs/`、`HybridMaxSAT/runs/`、`__pycache__/`、`*.pyc`、`*.orig`、`*.rej`、`.claude/settings.local.json`、Word 临时文件 `~$*`、本地临时目录 `.codex-tmp/` 与第三方源码 `scipoptsuite-8.1.0/`。其中 `.claude/settings.local.json` 为各机私有配置，已从版本控制中移除。

必须版本化的实验资产（不得忽略）：`scripts/run_batch.py`、`manifests/`、`tests/`、`experiment.md`、`docs/experiments/`。

## 研究目标

研究加权部分 MaxSAT（WPMS）：最小化未满足软子句的总权重。CASHWMaxSAT-DisjCad-S6 是精确求解器，负责维护下界（LB）、处理精确推理并证明最优；SPBMAXSAT2 是局部搜索求解器，负责寻找可行解并返回其目标值上界（UB）。

SPB 是 CASH 主模块调用的局部搜索接口：CASH 提供初始解，SPB 在给定窗口中搜索并只返回数值 UB。CASH 依据自身既有规则接收该 UB、收紧并继续推理。协同层不规定硬化公式，不传递 SPB 的完整赋值、子句集合或证明，也不把 SPB 的结果写回 CASH 的变量赋值。SPB 给出的 UB 可能帮助 CASH 达到 `LB = UB`；只有 CASH 确认 `LB = UB` 时，实例才算已证明最优。

## 已确定的协同协议

1. 每个实例先运行 CASH 一个窗口（当前为 30 秒）。
2. CASH 在安全检查点暂停，从当前仍有效的传播推理闭包中导出原始 WCNF 变量。只导出传播推理值；不导出分支决策、松弛变量、辅助变量或已被回溯的值。
3. CASH 将未推理的原始变量写为 `-1`，调用 SPB 既有 `improve()` 接口；SPB 内部随机补全这些位置。推理值仅构成 SPB 的初始解，SPB 在其窗口（当前为 10 秒）内可以翻转它们；翻转绝不写回 CASH。
4. 每个实例仅创建一个 SPB 对象，跨轮保留自适应子句权重，但不保留前一轮的赋值。
5. SPB 完整运行一个窗口后，只回传该轮最佳数值 UB。若没有严格改善 CASH 的当前 UB，丢弃该值并恢复 CASH 原状态；若改善，则以最小回调交给当前暂停的 CASH。
6. CASH 从相同的搜索状态继续，不做常规重启。CASH 一旦证明最优，立即结束该实例，不再启动 SPB。

为支持此协议，CASH 只允许加入很薄的协同回调：在安全检查点导出上述部分赋值、接收数值 UB，并从相同状态继续。不要改写其精确搜索、下界、核心或硬化逻辑。

## 时间与随机性

- 每实例端到端预算为 600 秒，计入 CASH、SPB 和交接开销。
- 时间片为 **CASH 30 秒 / SPB 10 秒**；SCIP 时限随 CASH 窗口，同为 30 秒。
- 600 秒到达后，不启动下一个模块；当前模块完整结束后停止。若当前模块为 SPB，则接收其 UB 一次后结束；若当前模块为 CASH，则完成该 30 秒窗口后直接结束。
- 50 个调试实例也使用 600 秒和完整协议。
- 调试集抽样种子为 `20260909`。每实例的随机序列由该全局种子与实例相对路径导出。
- 调试阶段只运行 **CASH 与 Hybrid 两个配置，且只使用种子 `20260909`**（50 实例 × 2 配置 = 100 次运行）。SPB、HybridNoInference、HybridNatural、HybridAdaptive 一律不在调试阶段运行。
- 正式实验每个实例、每种配置运行三次，种子为 `20260909`、`20260910` 和 `20260911`。同一实例、同一重复中的 SPB、混合和无推理初始化消融使用相同的实例派生种子。

### 时间片为何从 15/3 改为 30/10（2026-09-11）

- 全量实验（`runs/full2cfg` 与 `runs/full23w_1seed`）显示，抢占式 15/3 会打断 CaDiCaL 的搜索：266 次真正发生过交接的运行中 **0 次**证明最优，而相同实例上纯 CASH 证明了约 100 次。15/3 因此不能作为正式配置。
- 逐轮探针（`runs/probe_steps`、`runs/probe_3010` 的 `spb_steps` / `spb_init_seconds` / `spb_setup_seconds`）显示，15/3 下大实例的 SPB 窗口被固定开销吃光：第一次交接要付 2.5~5.8 秒把实例深拷贝进持久 worker，之后每轮还要付 1.0~3.0 秒的 `init()` 全量重建。于是第一轮在第一个时间检查点（第 1000 步）就退出，事件流记为 **999 次翻转**，实际几乎没有搜索。
- 改成 30/10 后，除最大实例的第一轮外每个窗口都真正在搜索：大实例每窗口 150 万~650 万次翻转；15/3 下前 300 秒里 14~16 轮全部空转的实例，30/10 下 3 轮就交回可行解并被 CASH 接受。
- 两个已知但尚未修复的问题，复现时须知：2.5 GB 级实例的第一轮（深拷贝 7.5 秒 + `init()` 3.1 秒）仍会超过 10 秒窗口；`init()` 每轮仍占窗口的 20%~50%，因为它做全量重建而不按赋值差异增量更新。

## 数据与实验阶段

调试阶段从 `/data/dataset/Maxsat/Complete/MSE23W` 均匀抽取 50 个实例。若候选无法被任一求解器正确解析，记录原因、跳过并从剩余文件补抽，直到有 50 个可运行实例。调试批次只跑 CASH 与 Hybrid 两个配置、只用种子 `20260909`（共 100 次运行），SPB 与各消融配置留到正式阶段。调试批次一旦发生崩溃、解析错误、非法界值或日志不一致，立即停止，修复后从头重跑。

调试通过后先生成报告，再由用户检查；确认后扩展到 `MSE23W` 与 `MSE24W` 的全部加权实例。正式核心配置是原始 CASH、独立 SPB、CASH+SPB 混合；另加入“无推理初始化”消融，其 SPB 每轮全部随机初始化。不要增加“重置 SPB 自适应子句权重”的消融。

## 实现与日志规则

新代码放在 `HybridMaxSAT/`：协同调度、CASH 最小接口封装、SPB 库驱动、解析/实验脚本、配置和日志都位于此目录。尽量保持两个原始求解器目录独立。

现有 SPB 的 `improve()` 会在每次调用时重建工作器，因而会丢失自适应子句权重。允许在其封装层增加一个跨轮持有 `LSworker` 的最小扩展，以保留该状态；不得修改局部搜索策略或权重更新公式。

新一轮初始解到达时，仅保留自适应子句权重；必须按新赋值重新计算评分、子句满足计数和候选变量栈。

修复混合流程前，必须先修复 SPB 的标准 WCNF 解析：先读取 `p wcnf` 头再分配存储，并正确处理数值 `top` 和 `h` 硬子句。用 CASH 已知结果交叉检查其目标值与硬/软子句处理。该 Parser Gate 未通过时，不得启动数据集实验。

每一轮日志至少记录：轮次、CASH 累计时间、导出的推理变量数量、调用 SPB 前的 CASH UB、SPB 本轮最佳 UB 和 CASH 接收后的 UB。还要记录 SPB 是否真的搜索过：翻转步数、`tries` 次数，以及窗口内 worker 构造、局部搜索（含 `init()`）和证书复核算三个阶段各自的耗时——只有这样才能区分「窗口在搜索」与「窗口被初始化吃光」。每次运行还要记录实例、配置、种子、最终 LB/UB、是否由 CASH 证明最优、端到端耗时、退出状态和错误信息。

## 变换正确性：不可放宽的约束

CASH 会把原 WCNF 的软子句转为松弛文字和 PB/SAT 约束，并在其内部使用 `fixed_goalval`、`goal_gcd`、`best_goalvalue`、`LB_goalvalue`、`UB_goalvalue` 等变换后的表示进行硬化。SPB 的原始 WCNF 代价不能直接写入这些内部字段；更不能由协同层自行硬化子句。

为保持结果正确，SPB 接口对 CASH 仍只输出数值 UB，但它在输出前必须私有地保留完整模型，并用已通过 Parser Gate 的原始 WCNF 语义确认：模型长度正确、所有硬子句满足、重算的软子句代价等于拟输出 UB。模型不写入 CASH 的搜索变量。若最终需要输出模型，结果层只能使用这份私有证书或由 CASH 自己重新产生同代价模型。

外部 UB 接口只能在 CASH 完成目标变换后、一次 `satSolveLimited()` 完整返回并完成状态处理的安全点执行。它必须通过唯一且受测试的“原始 WCNF 代价 → CASH 当前内部目标单位”转换路径更新正常的界处理入口；禁止直接赋值 `best_goalvalue`、`UB_goalvalue`、`LB_goalvalue` 或手写硬化规则。外部 UB 永远不能单独宣布最优，只有 CASH 的精确推理可以使 `LB = UB`。

服务器测试必须覆盖：标准 WCNF 解析等价性、SPB 模型与 UB 等价性、外部 UB 单位转换、严格改善/非改善处理、过小 UB 的拒绝、每种 CASH 变换分支下的硬化一致性，以及最终 `LB = UB` 时目标和模型的一致性。
