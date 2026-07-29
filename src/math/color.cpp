#include "pt/math/color.hpp"
#include "pt/math/random.hpp"
#include "pt/math/scalar.hpp"

namespace pt {

color color::random() {
    return color(random_scalar(), random_scalar(), random_scalar());
}

color color::random(Float min, Float max) {
    return color(random_scalar(min, max), random_scalar(min, max), random_scalar(min, max));
}

} // namespace pt
