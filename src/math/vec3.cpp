#include "pt/math/vec3.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/random.hpp"
#include "pt/math/scalar.hpp"
#include <cmath>
#include <limits>

namespace pt {

vec3 vec3::random() {
    return vec3(random_double(), random_double(), random_double());
}

vec3 vec3::random(Float min, Float max) {
    return vec3(random_double(min, max), random_double(min, max), random_double(min, max));
}

vec3 random_unit_vector() {
    while (true) {
        auto p = vec3::random(-1, 1);
        auto lensq = p.length_squared();

        if (lensq <= 1 && lensq > std::numeric_limits<Float>::min()) return p / std::sqrt(lensq);
    }
}

vec3 random_in_unit_disk() {
    while (true) {
        auto p = vec3(random_double(-1, 1), random_double(-1, 1), 0);
        const auto lensq = p.length_squared();

        if (lensq < 1) return p;
    }
}

vec3 random_cosine_direction() {
    const Float r1 = random_double();
    const Float r2 = random_double();
    const Float sqrt_r2 = std::sqrt(r2);

    const Float phi = 2.0_f * pi * r1;
    const Float x = std::cos(phi) * sqrt_r2;
    const Float y = std::sin(phi) * sqrt_r2;
    const Float z = std::sqrt(1.0_f - r2);

    return vec3(x, y, z);
}

} // namespace pt
