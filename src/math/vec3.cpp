#include "pt/math/vec3.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/random.hpp"
#include <cmath>

namespace pt {

vec3 vec3::random() {
    return vec3(random_double(), random_double(), random_double());
}

vec3 vec3::random(double min, double max) {
    return vec3(random_double(min, max), random_double(min, max), random_double(min, max));
}

vec3 random_unit_vector() {
    while (true) {
        auto p = vec3::random(-1, 1);
        auto lensq = p.length_squared();

        if (lensq <= 1 && lensq > 1e-160) return p / std::sqrt(lensq);
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
    const double r1 = random_double();
    const double r2 = random_double();
    const double sqrt_r2 = std::sqrt(r2);

    const double phi = 2.0 * pi * r1;
    const double x = std::cos(phi) * sqrt_r2;
    const double y = std::sin(phi) * sqrt_r2;
    const double z = std::sqrt(1.0 - r2);

    return vec3(x, y, z);
}

} // namespace pt
