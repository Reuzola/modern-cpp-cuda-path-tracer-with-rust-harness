#include "pt/math/vec3.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/random.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include <cmath>
#include <limits>

namespace pt {

Vec3 Vec3::random() {
    return Vec3(random_scalar(), random_scalar(), random_scalar());
}

Vec3 Vec3::random(Float min, Float max) {
    return Vec3(random_scalar(min, max), random_scalar(min, max), random_scalar(min, max));
}

Vec3 Vec3::random(Sampler& sampler) {
    const Float x = sampler.next_scalar();
    const Float y = sampler.next_scalar();
    const Float z = sampler.next_scalar();
    return Vec3(x, y, z);
}

Vec3 Vec3::random(Float min, Float max, Sampler& sampler) {
    const Float x = sampler.next_scalar(min, max);
    const Float y = sampler.next_scalar(min, max);
    const Float z = sampler.next_scalar(min, max);
    return Vec3(x, y, z);
}

Vec3 random_unit_vector() {
    while (true) {
        auto p = Vec3::random(-1, 1);
        auto lensq = p.length_squared();

        if (lensq <= 1 && lensq > std::numeric_limits<Float>::min()) return p / std::sqrt(lensq);
    }
}

Vec3 random_in_unit_disk() {
    while (true) {
        auto p = Vec3(random_scalar(-1, 1), random_scalar(-1, 1), 0);
        const auto lensq = p.length_squared();

        if (lensq < 1) return p;
    }
}

Vec3 random_cosine_direction() {
    const Float r1 = random_scalar();
    const Float r2 = random_scalar();
    const Float sqrt_r2 = std::sqrt(r2);

    const Float phi = 2.0_f * pi * r1;
    const Float x = std::cos(phi) * sqrt_r2;
    const Float y = std::sin(phi) * sqrt_r2;
    const Float z = std::sqrt(1.0_f - r2);

    return Vec3(x, y, z);
}

Vec3 random_unit_vector(Sampler& sampler) {
    while (true) {
        const Vec3 p = Vec3::random(-1, 1, sampler);
        const Float lensq = p.length_squared();

        if (lensq <= 1 && lensq > std::numeric_limits<Float>::min()) return p / std::sqrt(lensq);
    }
}

Vec3 random_in_unit_disk(Sampler& sampler) {
    while (true) {
        const Float px = sampler.next_scalar(-1, 1);
        const Float py = sampler.next_scalar(-1, 1);

        const Vec3 p = Vec3(px, py, 0);
        const Float lensq = p.length_squared();

        if (lensq < 1) return p;
    }
}

Vec3 random_cosine_direction(Sampler& sampler) {
    const Float r1 = sampler.next_scalar();
    const Float r2 = sampler.next_scalar();
    const Float sqrt_r2 = std::sqrt(r2);

    const Float phi = 2.0_f * pi * r1;
    const Float x = std::cos(phi) * sqrt_r2;
    const Float y = std::sin(phi) * sqrt_r2;
    const Float z = std::sqrt(1.0_f - r2);

    return Vec3(x, y, z);
}

} // namespace pt
