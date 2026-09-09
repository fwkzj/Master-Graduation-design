#ifndef UTIL_RANDOM_H
#define UTIL_RANDOM_H

#include <random>
#include <algorithm>

namespace util {

// 线程局部的全局随机数引擎，默认用随机设备播种。
inline std::mt19937_64 &rng() {
    static thread_local std::mt19937_64 eng{std::random_device{}()};
    return eng;
}

// 显式设置种子，方便复现实验。
inline void set_seed(std::uint64_t seed) {
    rng().seed(seed);
}

// 生成闭区间 [l, r] 上的整数。
inline int rand_int(int l, int r) {
    std::uniform_int_distribution<int> dist(l, r);
    return dist(rng());
}

// 生成区间 [l, r) 上的浮点数。
inline double rand_double(double l = 0.0, double r = 1.0) {
    std::uniform_real_distribution<double> dist(l, r);
    return dist(rng());
}

// 对迭代器区间做随机打乱。
template <class It>
inline void shuffle(It first, It last) {
    std::shuffle(first, last, rng());
}

} // namespace util

#endif // UTIL_RANDOM_H
