#pragma once
#include "pt/math/scalar.hpp"
#include <random>

namespace pt {

class Sampler;

[[nodiscard]] std::mt19937& rng();

[[nodiscard]] Float random_scalar();

[[nodiscard]] Float random_scalar(Float min, Float max);

[[nodiscard]] int random_int(int min, int max);

} // namespace pt
