#pragma once
#include <random>

[[nodiscard]] inline double random_double() {
    thread_local static std::mt19937 generator{ std::random_device{}()};
    thread_local static std::uniform_real_distribution<double> distribution{ 0.0, 1.0}; // [0, 1)
    return distribution(generator);
}

[[nodiscard]] inline double random_double(double min, double max) {
    return min + (max - min) * random_double();
}

[[nodiscard]] inline int random_int(int min, int max) {
    return static_cast<int>(random_double(min, max + 1));
}