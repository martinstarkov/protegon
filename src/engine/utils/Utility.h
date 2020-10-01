#pragma once

#include <iostream>
#include <random>

#define LOG(x) { std::cout << x << std::endl; }
#define LOG_(x) { std::cout << x; }

namespace internal {

    template <typename Floating, std::enable_if_t<std::is_floating_point<Floating>::value, int> = 0>
        Floating GetRandomValue(Floating min_range, Floating max_range) {
        std::random_device rd;
        std::mt19937 rng(rd());
        std::uniform_real_distribution<Floating> distribution(min_range, max_range);
        return distribution(rng);
    }
    template <typename Integer, std::enable_if_t<std::is_integral<Integer>::value, int> = 0>
        Integer GetRandomValue(Integer min_range, Integer max_range) {
        std::random_device rd;
        std::mt19937 rng(rd());
        std::uniform_int_distribution<Integer> distribution(min_range, max_range);
        return distribution(rng);
    }

} // namespace internal