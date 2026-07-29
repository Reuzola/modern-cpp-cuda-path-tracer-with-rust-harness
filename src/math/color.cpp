#include "pt/math/color.hpp"
#include "pt/math/random.hpp"
#include "pt/math/scalar.hpp"

namespace pt {

Color Color::random() {
    return Color(random_scalar(), random_scalar(), random_scalar());
}

Color Color::random(Float min, Float max) {
    return Color(random_scalar(min, max), random_scalar(min, max), random_scalar(min, max));
}

} // namespace pt
