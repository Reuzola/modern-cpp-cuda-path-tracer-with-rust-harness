#include "pt/math/color.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"

namespace pt {

Color Color::random(Sampler& sampler) {
    const Float r = sampler.next_scalar();
    const Float g = sampler.next_scalar();
    const Float b = sampler.next_scalar();
    return Color(r, g, b);
}

Color Color::random(Float min, Float max, Sampler& sampler) {
    const Float r = sampler.next_scalar(min, max);
    const Float g = sampler.next_scalar(min, max);
    const Float b = sampler.next_scalar(min, max);
    return Color(r, g, b);
}

} // namespace pt
