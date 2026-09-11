# 2026-09-11 deadline 只缩不伸：第一次交接之后 CASH 被完全饿死

日期：2026-09-11 ・ 机器：S122 ・ 修复提交 `1bb0277` ・ 验证批次 `runs/debug50_deadlinefix`

## 1. 现象

带 LB/UB 记录的批次 `runs/debug50_lbub` 显示：只要协议发生过交接，混合就再也没证明过任何实例，
而且 23 个交接运行里有 20 个的 CASH 下界在第一轮之后完全不动。逐轮原始记录（`tcp_students_112_it_2`）：

```
r1  t= 30.0  lb 3255  ub 3996   spb_ub 3660  accepted
r2  t= 70.0  lb 3255  ub 3660   spb_ub 3699  not_improved
...
r15 t=590.0  lb 3255  ub 3660   spb_ub -1    not_improved
```

同实例纯 CASH 在同一时间把 lb/ub 从 261/67281 推到 3585/3585 并证明最优（267 秒）。

## 2. 根因：截止时间只能被提前，从不被推后

```cpp
void set_cash_deadline(double absolute_wall_seconds) {
    const double current = g_cash_deadline.load(std::memory_order_relaxed);
    if (absolute_wall_seconds < current)        // 只收紧，不放松
        g_cash_deadline.store(absolute_wall_seconds, std::memory_order_relaxed);
}
```

每一轮结束后的 `arm_deadline()` 想把截止时间设成 `now + 30s`，但存进去的仍是**第一个窗口的 30 秒处**
（早已过期），写入被条件挡掉。此后 `cash_deadline_reached()` 永远返回「已超时」，
**CASH 的每一次 SAT 调用都在入口即被判超时**，循环空转。

实测指纹（本批次为验证而新增的 `cash_deadline_in` 字段，表示发起本轮时截止时间还剩多少秒）：

| 版本 | 第 1 轮 | 第 2 轮 |
|---|---|---|
| 修复前 | -0.01s | **-40.01s** |
| 修复后 | -0.01s | -0.00s |

修复前第 2 轮已经是负数 40 秒，等于第二个 CASH 窗口（t=40..70）里一次有效的 SAT 调用都没有。

## 3. 影响范围

所有使用抢占式 `Hybrid` 的批次都受影响：`runs/debug50`、`runs/full23w`、`runs/full2cfg`、
`runs/debug50_c30s10`、`runs/debug50_lbub`。它们关于「混合不如纯 CASH」的结论**作废**，
因为那些运行里 CASH 在第一次交接之后根本没有得到可用时间。

`HybridNatural`（非抢占）不受影响：它把截止时间设为常量（总预算），条件判断永远通过，
所以它在调试集上「0 亏 0 赚」是可信的。

## 4. 修复

```cpp
// 每轮重新武装窗口边界，允许把截止时间推后
void arm_cash_deadline(double absolute_wall_seconds) {
    g_cash_deadline.store(absolute_wall_seconds, std::memory_order_relaxed);
}
```

`set_cash_deadline()` 保留「只收紧」语义供预算使用；窗口武装改用 `arm_cash_deadline()`。
同时把 `cash_deadline_in` 固化进每轮事件，这类错误以后一眼可见。

## 5. 修复后的结果（`runs/debug50_deadlinefix`，50 实例，30/10，单种子）

绑定提交 `1bb0277`，100/100 完成，0 条 FAIL。

| 配置 | 证明最优 | 平均墙钟 |
|---|---|---|
| CASH | 36/50 | 206 s |
| **Hybrid** | **37/50** | 203 s |

成对比较：共同 36，仅 CASH 证明 **0**，仅 Hybrid 证明 **1**（`shiftdesign_..._inst-025_30m`，纯 CASH 600 秒未证明）。
修复前不证明、修复后证明的实例共 **10 个**。

其他读数：

- 发生交接的运行 19 个，其中 6 个证明最优（修复前 0/23）。
- CASH 的 LB 在交接之间继续推进的运行：**18/19**（修复前 3/23）。
- 233 轮里 SPB 返回可行解 143 轮（61%），被 CASH 接受 29 轮（12.4%）。
- 两边都证明的 36 个实例中，Hybrid 更快 12 个，平均时间差 +1 秒；典型如
  `tcp_students_112_it_2`：纯 CASH 270 秒证明，Hybrid 205 秒（快 65 秒）。
  也有变慢的（`drmx-cryptogen_1`：55 秒对 66 秒），来自每轮 10 秒的 SPB 时间开销。
- `cash_deadline_in` 中位数 -0.02 秒，说明窗口边界按计划生效；仍有 15 个轮次样本低于 -1 秒
  （最小 -23.93 秒），属于「个别 SAT 调用没能被终结器及时打断」的已知限制，与本次 bug 无关。

## 6. 结论

- 修复前观察到的「混合远不如纯 CASH」是**实现缺陷**造成的，不是协议性质：那些运行里 CASH 在第一次
  交接之后被完全饿死，LB 与 UB 自然都不会动。
- 修复后协议按设计工作：CASH 每个窗口都能推进下界，SPB 在窗口之间降低上界，
  两者交替进行；混合在 50 实例上不再输掉任何证明，并多证明一个实例。
- 下一步需要在同为 30/10 的配置下重跑更大的集合（以及非抢占对照），确认这个结论在规模上成立。
