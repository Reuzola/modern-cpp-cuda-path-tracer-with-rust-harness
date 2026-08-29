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
    const Vec3 reflected = reflect(unit_vector(r_in.direction()), rec.normal);

    // A polished metal takes no offset, so it draws nothing: random_unit_vector rejects
    // samples, and letting it run would tie the RNG stream to how many candidates it happened to discard.
    const Vec3 scattered_direction = fuzz_ > 0.0_f ? reflected + fuzz_ * random_unit_vector(sampler) : reflected;

    if (dot(scattered_direction, rec.normal) > 0)
        return ScatterRecord{.attenuation = albedo_, .bounce = SpecularBounce{.scattered = Ray(rec.p, scattered_direction, r_in.time())}};

    return std::nullopt;
}

} // namespace pt
