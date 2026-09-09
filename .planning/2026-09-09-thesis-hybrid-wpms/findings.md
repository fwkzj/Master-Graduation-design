# Findings: Thesis Research

## Confirmed Technical Facts

- CASHWMaxSAT-DisjCad-S6 是当前项目中的精确 WPMS 组件；命令行可输出目标值、状态和二进制模型。
- SPBMAXSAT2 的 `LocalSearchSolver` 是单线程同步库接口，可返回 `Solution` 和 1-based 赋值；命令行仅打印成本。
- SPB 当前标准 WCNF 解析有前置分配与 `h` 硬子句语义问题，尚不可直接用于公共 MSE 数据。
- `/data/dataset/Maxsat` 是可读的公共基准库，包含 Complete/Incomplete、MSE17–MSE26 等套件。
- SPB 的 `improve()` 可接收 1-based 的完整或部分赋值；未确定变量使用 `-1`，因此适合作为精确侧投影的接收端。
- 在最小化违规软权重的 WCNF 表示下，局部搜索可行解提供 UB；若论文采用最大化满足软权重的表述，则同一量可记作 LB，必须显式换算，不能混用两种界。
- “必须满足”只能由精确推理证明；局部搜索输出的是候选或偏好，不能提升证明意义上的 LB。

## Research Position

- 一次性热启动本身不足以构成创新；可检验贡献是经独立验证的状态接口、可解释的调度和严格消融。
- 任何性能提升必须在冻结测试集、统一资源限制和完整日志下报告；负结果同样是研究结果。

## Evidence Needed Before Claiming Results

- 独立验证器和 SPB 解析回归测试。
- CASH 边界注入的实际语义测试。
- B0/B1/H0/H1/H2 完整实验矩阵。
- 统计与分层分析、原始配置和日志。
