#include "pt/math/color.hpp"
#include "pt/math/random.hpp"
#include "pt/math/scalar.hpp"

namespace pt {

color color::random() {
    return color(random_double(), random_double(), random_double());
}

color color::random(Float min, Float max) {
    return color(random_double(min, max), random_double(min, max), random_double(min, max));
}

} // namespace pt
