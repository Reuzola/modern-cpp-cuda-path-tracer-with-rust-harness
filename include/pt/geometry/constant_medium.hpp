#pragma once
#include "pt/geometry/top_level_bvh.hpp"
#include "pt/math/scalar.hpp"
#include <optional>

namespace pt {

class Material;
class Ray;
class Sampler;
class Interval;
struct HitRecord;

class ConstantMedium final {
public:
    // The boundary is a tree of its own, never part of the world: a volume is not a surface.
    ConstantMedium(TopLevelBvh boundary, Float density, const Material* phase_function);

    // The distance along r to a scattering event inside ray_t, or nothing when the ray misses the volume or crosses
    // its part of ray_t unscattered. Draws once whenever that part is not empty. Fills no record: a nearer event, in
    // another medium or on a surface, may still take its place.
    [[nodiscard]] std::optional<Float> sample_distance(const Ray& r, const Interval& ray_t, Sampler& sampler) const;

    // Writes the record of a scattering event at t along r: a point with no surface, and the phase function.
    void fill(const Ray& r, Float t, HitRecord& rec) const noexcept;

private:
    TopLevelBvh boundary_;
    Float neg_inv_density_{};
    const Material* phase_function_;
};

} // namespace pt
