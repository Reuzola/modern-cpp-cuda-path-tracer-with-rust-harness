#pragma once
#include "pt/math/scalar.hpp"
#include <random>

namespace pt {

[[nodiscard]] std::mt19937& rng();

[[nodiscard]] Float random_double();

[[nodiscard]] Float random_double(Float min, Float max);

[[nodiscard]] int random_int(int min, int max);

} // namespace pt
