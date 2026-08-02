#pragma once
#include "pt/math/scalar.hpp"

namespace pt {

class Film;

enum class ToneMapOperator { none,
                             reinhard,
                             aces };

struct ToneMapSettings {
    Float exposure{1.0_f};
    ToneMapOperator op{ToneMapOperator::none};
};

[[nodiscard]] Film tone_map(const Film& film, const ToneMapSettings& settings);

} // namespace pt
