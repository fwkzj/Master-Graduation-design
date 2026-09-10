# 融合局部搜索的精确 MaxSAT 求解

本仓库用于硕士学位论文研究：以 CASHWMaxSAT 为精确求解核心，通过受验证的接口调用 SPBMAXSAT2 局部搜索，在保持精确性证明责任不变的前提下共享部分赋值和候选上界。

## 目录结构

```text
.
├── CASHWMaxSAT-DisjCad-S6/       # 精确 MaxSAT 求解器
├── SPBMAXSAT2-master/            # 局部搜索求解器
├── HybridMaxSAT/                 # CASH–SPB 协同层与运行器
├── docs/
│   ├── reports/                  # 开题、中期材料与提交稿
│   ├── literature/               # 论文原文与精读清单
│   ├── project/                  # 项目使用说明
│   └── archive/                  # 已停用但保留的历史文件
├── artifacts/report-analysis/    # 报告转换、渲染与检查产物
├── .planning/                    # planning-with-files 活动研究计划
├── agent.md                      # 工作环境、正确性和执行约束
├── plan.md                       # 算法实施与实验计划
└── CMakeLists.txt                # 服务器端统一构建入口
```

## 执行环境

源码编辑和版本控制可在本地进行。编译、测试和实验只在服务器 S122 的 `/home/fwkzj/HybridAlgorithm` 中执行；基准数据只读使用 `/data/dataset/Maxsat/Complete/`。

当前算法与实验约束以 `agent.md`、`plan.md` 和 `.planning/2026-09-09-thesis-hybrid-wpms/` 中的活动规划为准。
