# 进度

## 已完成

- 审阅 CASHWMaxSAT 与 SPBMAXSAT2 的现有接口，复现 SPB 标准 WCNF 解析缺陷。
- 与用户逐项确定混合协议、预算、数据集、重复、消融、日志和验收规则。
- 以最新结论重写 `agent.md`、`plan.md` 与本规划目录。
- 为 SPB 标准 WCNF 头解析加入未执行的 Parser Gate 回归测试；按工作约定，该测试只能在 S122 恢复后运行。
- 核心算法设计已冻结：CASH 为主模块，SPB 是“初始解输入、数值 UB 输出”的 3 秒接口；后续不再以设计问题阻塞实现。
- Parser Gate 的标准 WCNF 与 `h` 硬子句回归测试已写入；`Instance::build_instance()` 已改为先读头部、后分配、统一硬子句语义。尚未在本地执行。
- SPB 新增了独立的跨轮持久接口，并添加其服务器回归测试：一个对象持有同一工作器的自适应权重；每轮仍用新的部分赋值重建搜索状态。既有 `improve()` 不变。
- CASH 新增了仅供混合调度器设置的安全点回调。它在目标变换完成后的主循环中导出原变量的传播快照，并拒绝负值、单位不一致或低于 LB 的外部 UB；接受的 UB 只进入 CASH 的内部界状态，后续硬化仍使用其现有函数。
- 已创建 `HybridMaxSAT/` 协调器库。它持有单个 SPB 对象和 SPB 私有模型证书，按固定窗口调用持久接口，记录轮次、传播变量数和各侧 UB；尚未生成运行器或在本地构建。
- Parser Gate 扩展为覆盖空软/空硬子句；SPB 已把空软子句作为固定目标代价处理，并拒绝空硬子句。所有新增测试等待服务器执行。
- 已添加顶层 CMake 入口和 `hybridmaxsat` 运行器。运行器读取一个 WCNF，执行固定混合协议，把逐轮事件写入 JSONL，并输出最终 LB、UB、incumbent、SPB 证书 UB 和 CASH 最优性状态。CASH CMake 库源清单也已改为链接其实际的公共实现而非 `main`。

## 当前进行中

- 本地按阶段实现 Parser Gate、SPB 持久接口与 CASH 最小回调，暂不执行。
- 在服务器恢复后，先编译并运行 Parser Gate 回归测试；通过前不进入混合调度测试。

## 下一步

1. 静态审阅 SPB 的持久接口、Parser Gate 及其构建配置；不在本地编译或运行。
2. 为 CASH UB 单位转换/拒绝规则（含空软子句）补充服务器测试，并审阅顶层 CMake 集成。
3. 服务器恢复后同步并执行 Parser Gate、接口测试、构建检查和 50 实例调试。

## 2026-09-10 仓库整理

- 已盘点顶层文件、目录规模、活动规划位置和构建路径依赖。
- 已确定保持算法源码目录稳定，将论文材料集中到 `docs/`，将报告分析产物集中到 `artifacts/`，并归档根目录旧规划副本。
- 已将开题提交稿迁入 `docs/reports/proposal/submissions/`，开题和中期模板分别迁入 `docs/reports/templates/proposal/` 与 `docs/reports/templates/midterm/`。
- 已将论文文献迁入 `docs/literature/papers/`，精读清单迁入 `docs/literature/reading-list/`。
- 已将 `.analysis_tmp` 迁入 `artifacts/report-analysis/`，并将根目录旧 planning-with-files 副本归档到 `docs/archive/legacy-planning/`。
- 路径扫描发现旧启动文件包含明文 API 凭据；正在改为无凭据环境配置示例，并记录撤销旧凭据的要求。
- 文档哈希检查发现一组重复论文 PDF；采用保留原文件并归档通用命名副本的方式处理。
- 已将含凭据的旧启动文件改为 `docs/project/claude-env.example.ps1`，当前版本只含占位符。
- 已移除迁移后留下的空 `报告要求` 目录，并补充根目录、文档目录和中间产物说明。
- 已将旧上传临时镜像移出项目根目录，避免嵌套仓库干扰；算法源码、文档、分析产物和活动规划层级均已核对。
- 已完成当前工作树凭据扫描、Git 重命名检查和暂存内容检查；未发现当前版本中的明文令牌或未纳入版本控制的文件。
- 仓库整理阶段完成，等待提交并同步到 GitHub。本地未编译、未运行测试或实验，符合 `agent.md` 的服务器执行约束。
