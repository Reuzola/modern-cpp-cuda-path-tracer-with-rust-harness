#include "pt/math/random.hpp"
#include "pt/math/scalar.hpp"
#include <random>

namespace pt {

std::mt19937& rng() {
    thread_local static std::mt19937 generator{std::random_device{}()};
    return generator;
}

Float random_scalar() {
    thread_local static std::uniform_real_distribution<Float> distribution{0.0_f, 1.0_f}; // [0, 1)
    return distribution(rng());
}

Float random_scalar(Float min, Float max) {
    return min + (max - min) * random_scalar();
}

int random_int(int min, int max) {
    const Float low = static_cast<Float>(min);
    const Float high = static_cast<Float>(max);
    return static_cast<int>(random_scalar(low, high + 1));
}

} // namespace pt
