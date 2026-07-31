#include "pt/materials/metal.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/color.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include <algorithm>
#include <optional>

namespace pt {

Metal::Metal(const Color& albedo, Float fuzz) : albedo_(albedo), fuzz_(std::clamp(fuzz, 0.0_f, 1.0_f)) {}

std::optional<ScatterRecord> Metal::scatter(const Ray& r_in, const HitRecord& rec, Sampler& sampler) const {
    auto reflected = reflect(unit_vector(r_in.direction()), rec.normal);
    auto scattered_direction = reflected + fuzz_ * random_unit_vector(sampler);

    if (dot(scattered_direction, rec.normal) > 0) {
        return ScatterRecord{.attenuation = albedo_, .bounce = SpecularBounce{.scattered = Ray(rec.p, scattered_direction, r_in.time())}};
    }
    return std::nullopt;
}

} // namespace pt
