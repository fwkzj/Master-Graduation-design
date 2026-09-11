# 硕士学位论文研究计划：面向 WPMS 的可验证混合求解

## Goal

完成一篇以“精确 MaxSAT 与局部搜索双向协同”为主题、论证可复现且实验结论可信的硕士学位论文：先建立正确的 WPMS 输入与候选验证基础，再评估局部搜索→精确求解的界传递和精确推理→局部搜索的部分赋值热启动，最后形成实现、实验、论文和答辩材料。

## Research Scope

- 问题：加权部分 MaxSAT（WPMS）。
- 精确组件：CASHWMaxSAT-DisjCad-S6；它承担最优性证明与精确状态责任。
- 局部搜索组件：SPBMAXSAT2；它提供候选模型，并接收完整或部分初始赋值。
- 核心贡献候选：独立候选验证、双向状态投影、分级界共享和可解释的时间片调度。
- 非目标：未经独立设计与验证，不改写 CASH 的核心引导/PB 推理或 SPB 的基本局部搜索策略。

## Research Questions

| ID | 问题 | 可证伪判据 |
|---|---|---|
| RQ1 | 经独立验证的 SPB 候选能否改善初始可行解质量或获得可行解的时间？ | 与相同资源限制下的 CASH 基线比较首次可行解时间与经验证 UB |
| RQ2 | 验证式热启动或串行 UB 共享能否改善精确求解结果？ | 比较证明最优数、最终 UB、运行时间与合法间隙 |
| RQ3 | 精确侧导出的原变量部分赋值能否改善 SPB 的局部搜索？ | 比较随机/无种子与精确种子下的首次可行时间、最终经验证 UB 和稳定性 |
| RQ4 | 基于可观测进展的确定性时间片调度是否优于固定时间分配？ | 自适应策略与固定策略的配对消融，并报告退化实例族 |
| RQ5 | 收益出现在哪些实例族、规模和权重结构上？ | 按数据集族、年份、加权类型与规模分层报告 |

## Non-negotiable Correctness Conditions

1. 原始 WCNF 中的硬/软子句与软权重是唯一目标语义来源。
2. 每个局部搜索模型必须由独立验证器检查硬可行性并重算软子句代价。
3. 只有严格改善且验证通过的候选才能更新 UB。
4. SPB 的内部动态权重、`bestcost` 和启发式评分不能直接作为 UB、LB 或证明。
5. 只有 CASH 的合法终止状态可标记为最优；达到时限只能标记为未知且附带最后经验证 UB。
6. 对最小化违反软权重的标准 WCNF，局部搜索可行解给出 UB；若以最大化已满足软权重描述，则同一赋值给出等价 LB，二者必须通过 `UB = total_soft_weight - LB` 显式换算。
7. 任何“某子句/变量必须满足”的结论都必须来自精确侧的可验证推理；局部搜索只能提出经验性偏好，不能证明必然性。

## Phases

### Phase 1: Research framing and source audit

**Status:** complete

- [x] 明确 WPMS、两个求解器角色和正确性边界。
- [x] 阅读 CASHWMaxSAT 与 SPBMAXSAT2 的入口、构建、模型输出和线程语义。
- [x] 确认公共 MSE 数据位置与规模。
- [x] 建立 `agent.md` 与 `plan.md` 的实验执行指南。

**Deliverables:** 源码审计结论、实验指南、论文题目候选、研究问题初稿。

### Phase 2: Parser Gate and semantic validation

**Status:** complete（2026-09-10 在 S122 通过；Stage 0 四配置界值复核全部与手算最优一致）

- [x] 为传统 WCNF 编写独立解析与候选验证器。
- [x] 修复 SPB 的标准 WCNF 头部读取、内存分配和硬子句分类。
- [x] 用手工小实例、CASH 样例和拒绝测试验证修复。
- [x] 定义支持的格式边界；将未支持的扩展 WCNF 明确拒绝。

**Exit criteria:** SPB 返回模型的独立成本与 CASH 小实例最优值一致或不低于最优值；当前 `CASH=1611 / SPB=0` 的样例差异消失。

### Phase 3: Reproducible baseline framework

**Status:** complete（2026-09-10/11；`manifests/`、`scripts/run_batch.py`、`runs/` 与 `meta.json` 已绑定提交与二进制 sha1）

- [x] 建立可版本化的构建、配置、实例清单和运行目录。
- [x] 实现 CASH 输出解析与模型复核。
- [x] 用 SPB 静态库驱动输出模型、种子和结构化统计。
- [x] 冻结调试集（`debug50_MSE23W`），保存哈希与分层信息。

**Exit criteria:** 同一配置、实例和种子可以重跑；B0（CASH）和 B1（SPB）产生完整结构化日志。

### Phase 4: Hybrid method implementation

**Status:** in_progress（H1/H2 已实现并跑完调试与全量；H3 的 `HybridAdaptive` 已实现，尚未全量评估）

- [x] 实现候选来源记录、拒绝规则与 CASH 原状态回退。
- [x] 实现 Exact-to-LS 投影：仅保留原变量，未确定变量用 `-1`。
- [x] 实现 H1：精确部分赋值作为 SPB 持久化局部搜索的初始解（含私有模型证书与原始 WCNF 复核）。
- [x] 实现 H2：计入重启成本的双向串行时间片共享（抢占式 15/3 与 30/10 均可运行）。
- [x] 实现 H3：只基于已公开、可记录指标的确定性调度（`HybridAdaptive` 退避）。
- [ ] 消除 SPB 窗口内的固定开销：持久 worker 预构造；`init()` 改为按赋值差异增量重建。

**Exit criteria:** 每层均有单元测试、端到端测试和相对于前一层的消融配置；任何异常都能退化到 CASH 基线。

### Phase 5: Development experiments and calibration

**Status:** in_progress（2026-09-11）

- [ ] 在开发集上确定总预算、时间片、停滞阈值、SPB 参数和种子数。
      —— 已把时间片从 15/3 调为 30/10，当前正用 `debug50_c30s10`（CASH+Hybrid、单种子）验证。
- [ ] 不查看测试集成绩的前提下冻结最终配置。
- [x] 记录负结果、失效实例和调参历史（见 `progress.md` 2026-09-11 与 `findings.md`）。

**Exit criteria:** 已冻结配置、参数选择理由和可复现实验清单。

### Phase 6: Final evaluation and analysis

**Status:** pending

- [ ] 在冻结测试集运行 B0、B1、H0、H1、H2。
- [ ] 统计证明最优数、首次可行时间、最终经验证 UB、时间、内存和间隙。
- [ ] 按实例族、加权类型和规模进行分层与配对分析。
- [ ] 形成消融、失败案例和有效性威胁分析。

**Exit criteria:** 原始日志齐全，表格和图可从配置自动重建，结论包含收益与退化两面。

### Phase 7: Thesis writing and revision

**Status:** pending

- [ ] 完成开题/中期所需版本：问题、动机、相关工作、方法与计划。
- [ ] 完成论文主体：理论与接口契约、实现、实验、讨论和结论。
- [ ] 完成图表、附录、复现说明、参考文献和术语统一。
- [ ] 导师反馈两轮、预答辩修订、最终格式检查与答辩幻灯片。

**Exit criteria:** 论文主张与冻结实验完全对应；任何未证实假设均不写为结果。

### Phase 8: Current midterm report revision

**Status:** complete

- [x] 盘点现有中期报告、开题报告和最新研究材料，确定可写事实与不可写实验结果。
- [x] 以开题报告为版式依据，核对字体、字号、标题层级、段落与页面设置。
- [x] 更新中期报告正文；实验进展暂不填写，并保留“当前实验效果”“已尝试改进方法”的后续填写位置。
- [x] 渲染并逐页检查最终 DOCX，修复排版问题。

**Exit criteria:** 中期报告内容与当前项目状态一致，未虚构实验结果，版式与开题报告一致，并通过逐页视觉检查。

## Timeline (24 weeks, adjustable to the school deadline)

| Weeks | Focus | Milestone |
|---|---|---|
| 1–2 | 研究问题、源码审计、相关工作阅读 | 研究边界与文献矩阵 |
| 3–5 | Parser Gate 与独立验证器 | 可用的标准 WCNF 管线 |
| 6–7 | 单求解器基线与数据划分 | 可复现 B0/B1 |
| 8–11 | H0/H1 与测试 | 经验证的混合原型 |
| 12–13 | H2 与开发集调参 | 冻结配置 |
| 14–17 | 最终实验与统计分析 | 可复现实验包 |
| 18–21 | 撰写、图表、讨论 | 完整初稿 |
| 22–24 | 修改、预答辩、定稿 | 提交稿与答辩材料 |

## Thesis Chapter Plan

1. 绪论：问题背景、WPMS、研究动机、贡献与论文结构。
2. 相关工作：精确/核心引导 MaxSAT、局部搜索 MaxSAT、协同或热启动方法；明确已有方法与本课题的差异。
3. 问题定义与设计原则：WPMS 语义、正确性契约、系统边界和研究问题。
4. 方法：独立验证器、SPB 适配、CASH 适配、局部搜索→精确界传递、精确→局部搜索部分赋值投影与分级调度。
5. 实现：解析器修复、接口、日志、资源治理、失败回退和复现环境。
6. 实验：基准、基线、配置、指标、结果、消融、实例族分析和有效性威胁。
7. 结论与展望：已验证结论、局限和后续接口/算法方向。

## Experiment Matrix

| ID | Method | Purpose |
|---|---|---|
| B0 | CASH only | 精确基线 |
| B1 | SPB only, multi-seed | 局部搜索质量与稳定性基线 |
| H0 | verified warm start | 初始 UB 的作用 |
| H1 | exact-to-SPB partial seed | 精确部分赋值热启动的作用 |
| H2 | fixed bidirectional serial slices | 双向协同的作用 |
| H3 | deterministic adaptive slices | 调度贡献 |

所有方法使用相同资源限制；混合方法的重启、解压、验证和调度时间都计入总成本。

## Risks and Decision Rules

| Risk | Response |
|---|---|
| SPB 解析修复后仍与独立验证不一致 | 停止实验，缩小修复范围并增加回归测试 |
| CASH 无法安全接收外部 UB | 保留 SPB/CASH 独立基线，只研究验证式候选与报告，不伪造在线集成 |
| CASH 命令行无法导出安全的中间部分赋值 | 先新增受测试的适配器；在此之前只使用其已完成模型，不能伪造“推理种子” |
| 数据格式多样 | 维护支持矩阵；不支持的格式明确排除 |
| 混合方法无提升或退化 | 如实报告，并分析何种实例/成本导致退化 |
| 资源或时间不足 | 缩小实例清单，优先保证正确性、可复现性和完整消融 |

## Next Step

1. 等 `runs/debug50_c30s10` 跑完，核对零崩溃/解析错误/非法界值，并用事件流里的 `spb_steps` 确认每一轮 SPB 是否真的搜索（判读规则见 `experiment.md` §8.2）。
2. 若第一轮仍被实例深拷贝吃掉（预计 781 MB 的 `shiftdesign` 与 486 MB 的 `downcast-pmd` 会命中），实施 Phase 4 最后一项结构修复并复测。
3. 修复后在 50 实例上比较 `spb_init_seconds` 与每窗口翻转数，再决定是否推广到全量。

## Repository Organization Maintenance (2026-09-10)

**Status:** complete

- [x] 保持三套算法源码目录和顶层 CMake 路径稳定。
- [x] 将开题、中期和文献材料归入统一的 `docs/` 层级。
- [x] 将报告分析中间产物从隐藏目录迁入 `artifacts/`。
- [x] 归档根目录重复的旧规划文件，保留 `.planning/` 为唯一活动规划源。
- [x] 更新 README、执行指南及所有受影响的路径引用。
- [x] 核对 Git 状态、文件清单和重命名结果。

## Decisions Made

| Decision | Rationale |
|---|---|
| 以 Parser Gate 作为首要技术里程碑 | 当前 SPB 对标准 WCNF 的错误会污染所有后续实验 |
| 将 CASH 设为唯一证明来源 | 当前接口和 MaxSAT 语义要求精确模块承担最优性责任 |
| 先做串行分级融合 | 现有 CASH 没有经验证的运行期热更新接口，避免虚构在线能力 |
| 调试阶段只跑 CASH 与 Hybrid、单种子 `20260909` | 全量已证明 SPB 与 HybridNoInference 不提供额外信息，多配置多种子只是重复成本 |
| 时间片由 15/3 改为 30/10（2026-09-11） | 15/3 下窗口被 worker 深拷贝与 `init()` 吃光，SPB 实际未搜索；且抢占式打断使 266 次交接 0 次证明 |
| 混合协议采用抢占式还是非抢占式暂不冻结 | 非抢占（`HybridNatural`）不亏也不赚，需在消除 SPB 窗口固定开销后重测才有意义 |

## Errors Encountered

| Error | Resolution |
|---|---|
| SPB 在 CASH 样例上报告 0，而 CASH 证明 1611 | 定位为解析/硬子句语义缺陷，纳入 Phase 2 Gate |
| 本机没有 `pdftotext`，无法读取重复 PDF 的标题页 | 不安装额外工具；按哈希确认内容相同，保留描述明确的文件名并将通用命名副本归档 |
| PowerShell 首次误解析未加引号的 Git tree-ish | 将 `HEAD^{tree}` 和 `origin/main^{tree}` 作为带引号参数重新执行，目录树哈希验证通过 |
| 文档渲染器首次运行未在 PATH 找到 `soffice.exe` | 正在定位工作区依赖中的捆绑 LibreOffice；不使用用户安装版替代 |
| 预期路径中不存在捆绑 `rg.exe` | 改用 Codex 应用随附的 `rg`，确认文档运行时中确实没有 LibreOffice |
| DOCX 检查脚本输出遇到 Windows GBK 无法编码零宽字符 | 将脚本标准输出显式设为 UTF-8 后重试 |
| 第一次压缩 DOCX 审计输出的补丁上下文不匹配 | 读取脚本现状后按实际导入位置重新应用补丁 |
| 动态样式审计误把字符样式当作段落样式 | 仅保留段落类型的标题和正文样式后重试 |
| 捆绑 Python 不含 PyMuPDF `fitz` | 使用已提供的 `pypdfium2` 将 Word 导出的 PDF 转为逐页 PNG |
| 首次生成修订稿时误在 `Run` 包装对象上调用底层属性方法 | 改为通过 `run._r.get_or_add_rPr()` 设置 OOXML 字体属性后重新生成 |
| 首轮视觉检查把以年份开头的正文误判为一级标题 | 将一级标题识别限定为 1 至 6 的单数字章节编号，并重新生成、渲染全文 |
| 第二轮视觉检查发现一级标题继承了旧稿的分页前属性，造成大面积页尾留白 | 在统一段落格式时显式取消分页前属性，再次生成并逐页验收 |
