#ifndef UTIL_TIMER_H
#define UTIL_TIMER_H

#include <chrono>

namespace util {

// 全局起始时间，按需延迟初始化。
inline std::chrono::steady_clock::time_point &global_start_time() {
    static std::chrono::steady_clock::time_point t = std::chrono::steady_clock::now();
    return t;
}

// 显式设置/重置全局起始时间（通常在 main 或 Solver::Solve 开头调用一次）。
inline void start_global_timer() {
    global_start_time() = std::chrono::steady_clock::now();
}

// 距离全局起始时间已经过去的秒数（double）。
inline double global_elapsed_seconds() {
    using namespace std::chrono;
    auto now = steady_clock::now();
    return duration_cast<duration<double>>(now - global_start_time()).count();
}

// 全局 cutoff 时间（秒），0 或负值表示未设置。
inline double &global_cutoff_seconds() {
    static double cutoff = 0.0;
    return cutoff;
}

// 设置全局 cutoff 时间（秒），例如 settings.cutoff_time。
inline void set_global_cutoff(double seconds) {
    global_cutoff_seconds() = seconds;
}

// 是否已经超过全局 cutoff 时间。
inline bool global_timeout() {
    double cutoff = global_cutoff_seconds();
    if (cutoff <= 0.0) return false; // 未设置 cutoff
    return global_elapsed_seconds() >= cutoff;
}

// 简单的作用域计时器，用于局部测量一段代码的耗时。
class ScopeTimer {
public:
    explicit ScopeTimer(double &accumulator)
        : acc_(accumulator), start_(std::chrono::steady_clock::now()) {}

    ~ScopeTimer() {
        using namespace std::chrono;
        auto end = steady_clock::now();
        acc_ += duration_cast<duration<double>>(end - start_).count();
    }

private:
    double &acc_;
    std::chrono::steady_clock::time_point start_;
};

// 简单的手动计时器，可独立于全局时间使用。
class ManualTimer {
public:
    ManualTimer() { reset(); }

    void reset() { start_ = std::chrono::steady_clock::now(); }

    double elapsed_seconds() const {
        using namespace std::chrono;
        auto now = steady_clock::now();
        return duration_cast<duration<double>>(now - start_).count();
    }

    double elapsed_milliseconds() const {
        using namespace std::chrono;
        auto now = steady_clock::now();
        return duration_cast<duration<double, std::milli>>(now - start_).count();
    }

private:
    std::chrono::steady_clock::time_point start_;
};

} // namespace util

#endif // UTIL_TIMER_H
