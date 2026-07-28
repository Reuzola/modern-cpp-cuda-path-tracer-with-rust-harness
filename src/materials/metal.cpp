#include "pt/materials/metal.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/vec3.hpp"
#include <algorithm>

namespace pt {

metal::metal(const color& albedo, double fuzz) : albedo(albedo), fuzz(std::clamp(fuzz, 0.0, 1.0)) {}

std::optional<scatter_record> metal::scatter(const ray& r_in, const hit_record& rec) const {
    auto reflected = reflect(unit_vector(r_in.direction()), rec.normal);
    auto scattered_direction = reflected + fuzz * random_unit_vector();

    if (dot(scattered_direction, rec.normal) > 0) {
        return scatter_record{.attenuation = albedo, .bounce = specular_bounce{.scattered = ray(rec.p, scattered_direction, r_in.time())}};
    }
    return std::nullopt;
}

} // namespace pt
