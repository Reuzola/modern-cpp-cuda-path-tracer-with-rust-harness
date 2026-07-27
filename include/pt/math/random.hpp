#pragma once
#include <random>

namespace pt {

[[nodiscard]] std::mt19937& rng();

[[nodiscard]] double random_double();

[[nodiscard]] double random_double(double min, double max);

[[nodiscard]] int random_int(int min, int max);

} // namespace pt
