#include "pt/math/random.hpp"
#include <random>

namespace pt {

std::mt19937& rng() {
    thread_local static std::mt19937 generator{std::random_device{}()};
    return generator;
}

double random_double() {
    thread_local static std::uniform_real_distribution<double> distribution{0.0, 1.0}; // [0, 1)
    return distribution(rng());
}

double random_double(double min, double max) {
    return min + (max - min) * random_double();
}

int random_int(int min, int max) {
    return static_cast<int>(random_double(min, max + 1));
}

} // namespace pt
