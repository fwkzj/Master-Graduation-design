# A Maxsat localsearch solver based on SPB-MAXSAT

## Usage

基本运行命令（在 build 目录下）：

```bash
./solver <input.wcnf> [options]
```

示例：

```bash
./solver /path/to/instance.wcnf -h_inc 28 -bms_num 97 -cutoff 20
```

其中可选参数含义如下（对应 Settings 结构体中的字段）：

- `-rdprob <float>`：随机从 goodvar 栈中选变量的概率，对应 `rdprob`（默认 0.5）。
- `-bms_num <int>`：局部搜索每次从 goodvar 栈中采样/扫描的变量数阈值，对应 `hd_count_threshold`。
- `-rwprob <float>`：从当前未满足子句中随机选变量的概率，对应 `rwprob`。
- `-hard_sp <float>`：对硬子句相关权重进行平滑的概率，对应 `smooth_probability`。
- `-soft_sp <float>`：对软子句权重进行平滑的概率，对应 `soft_smooth_probability`。
- `-soft_weight_threshold <double>`：软子句权重增加的上限阈值，对应 `softclause_weight_threshold`。
- `-h_inc <double>`：每次对未满足硬子句增加的权重量，对应 `h_inc`。
- `-s_inc <double>`：每次对未满足软子句增加的权重量，对应 `s_inc`。
- `-coe <int>`：软子句权重系数（无权情形下），对应 `coe_soft_clause_weight`。
- `-cutoff <int>`：运行时间上限（秒），对应 `cutoff_time`。
- `-pool_size <int>`：解池最多保存的完整解数量，对应 `solution_pool_size`（默认 15，最小 1）。

如果某个参数未通过命令行显式指定，则使用 [src/BasicStruct/settings.h](src/BasicStruct/settings.h) 中给出的默认值。

## 作为局部搜索库嵌入

构建会同时生成：

- `solver`：原有命令行程序；
- `spbmaxsat`：可以链接到其他 C++ 求解器的静态库目标。

如果两个项目由同一个 CMake 工程管理，可以直接使用：

```cmake
add_subdirectory(path/to/SPBMaxSAT2)
target_link_libraries(your_exact_solver PRIVATE spbmaxsat)
```

公共封装位于 `Solver/local_search_solver.h`，它是同步、单线程接口。最简单的调用方式如下：

```cpp
#include "Solver/local_search_solver.h"

Settings settings;
settings.cutoff_time = 10;
settings.solution_pool_size = 1; // 只需要一个 UB 时可降低内存占用

spbmaxsat::LocalSearchSolver local_solver("instance.wcnf", settings);
Solution result = local_solver.solve();

if (result.feasible) {
    long long upper_bound = result.cost;
    const std::vector<int>& assignment = result.assignment;
}
```

也可以从一个已有的 `Instance` 构造。封装会深拷贝该实例，调用方仍然拥有原实例：

```cpp
Instance instance;
instance.build_instance("instance.wcnf");

spbmaxsat::LocalSearchSolver local_solver(instance, settings);
Solution result = local_solver.solve();
```

### 导入初始解并尝试改进

赋值数组沿用求解器内部格式：长度必须为 `num_vars + 1`，下标 0 不使用。值 `0/1` 表示变量的初始真假值，`-1` 表示由局部搜索随机补全。初始值只是搜索起点，不会固定变量。

```cpp
std::vector<int> initial(instance.num_vars + 1, -1);
initial[1] = 1;
initial[2] = 0;

Solution improved = local_solver.improve(initial);
if (improved.feasible && improved.cost < current_upper_bound) {
    current_upper_bound = improved.cost;
    incumbent = improved.assignment;
}
```

`solve()` 和 `improve()` 都只运行一个局部搜索 worker。如果希望使用仓库现有的解池、约简和交叉流程，可以调用：

```cpp
Solution result = local_solver.solve_with_crossover();
```

封装返回前会使用原实例重新验证可行解及其代价；验证失败时抛出 `std::runtime_error`。初始解尺寸或取值错误时抛出 `std::invalid_argument`。

### 解池内存

每个池中解都保存一个完整的 `int` 赋值数组，因此主要开销大约为：

```text
solution_pool_size * 4 * num_vars 字节
```

默认池大小 15，即大约每个变量 60 字节。只用局部搜索提供或改进 UB 时，可将 `solution_pool_size` 设为 1；需要交叉搜索时建议至少设为 2。

当前计时器和随机数状态仍是进程级状态，所以该封装面向单线程、同步调用场景，不应由多个线程同时运行多个实例。

## Development Notes

### commit 585b3ef1fb3b0f135aa6271d9c26a2d588370ce0
Separate the instance part and setting part from SPB-MAXSAT.
Build LSworker class.

### commit 3ca44c3f897ddc25a3cef5beda1b77b3a529284c // f591e159b0b93020b346ddaa9dbda95d88899cf4
Add readme and instance reduce method。
