#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/geometry/instance.hpp"
#include "pt/geometry/mesh.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "support/test_support.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

// A brute-force reference for queries through instances, shared by the suites that build two-level trees.
// Everything here is inline: this header is included by more than one translation unit.
namespace pt_test {

// One object of the reference scan: a primitive as its own Hittable, and the instance it is seen through,
// if any. The scan moves the ray in and the record out for every candidate, the way nested Hittables did:
// an implementation independent of the trees' winner bookkeeping, sharing only the transform arithmetic.
struct Placed {
    const pt::Hittable* object = nullptr;
    std::optional<pt::Instance> instance;
};

// The nearest of all placed objects, and which one it was.
struct ReferenceHit {
    std::size_t index{};
    pt::HitRecord rec;
};

[[nodiscard]] inline std::optional<ReferenceHit> reference_hit(std::span<const Placed> placed, const pt::Ray& r, const pt::Interval& ray_t) {
    std::optional<ReferenceHit> nearest;
    pt::Float closest = ray_t.max;

    for (std::size_t i = 0; i < placed.size(); ++i) {
        const pt::Ray ray = placed[i].instance ? placed[i].instance->to_local(r) : r;

        pt::HitRecord rec;
        if (!placed[i].object->hit(ray, pt::Interval(ray_t.min, closest), rec)) continue;
        if (placed[i].instance) placed[i].instance->to_world(rec);

        closest = rec.t;
        nearest = ReferenceHit{.index = i, .rec = rec};
    }
    return nearest;
}

[[nodiscard]] inline bool same_vec(const pt::Vec3& a, const pt::Vec3& b) {
    return same_bits(a.x(), b.x()) && same_bits(a.y(), b.y()) && same_bits(a.z(), b.z());
}

// Bit for bit, field by field: the same primitive won, through the same instance, and was filled from the
// same ray. Any slip in the bookkeeping - a stale slot, the wrong instance, the world ray handed to an
// object-space fill - moves at least one of these.
[[nodiscard]] inline bool same_record(const pt::HitRecord& a, const pt::HitRecord& b) {
    return same_bits(a.t, b.t) && a.mat == b.mat && same_vec(a.p, b.p) && same_vec(a.normal, b.normal) &&
           same_vec(a.geometric_normal, b.geometric_normal) && same_bits(a.spawn_margin, b.spawn_margin) &&
           a.front_face == b.front_face && same_bits(a.u, b.u) && same_bits(a.v, b.v);
}

// Braced, so the three draws happen left to right whatever the compiler.
[[nodiscard]] inline pt::Point3 point_in(pt::Sampler& sampler, const pt::Aabb& bounds) {
    return pt::Point3{sampler.next_scalar(bounds.x.min, bounds.x.max), sampler.next_scalar(bounds.y.min, bounds.y.max),
                      sampler.next_scalar(bounds.z.min, bounds.z.max)};
}

[[nodiscard]] inline pt::Ray ray_into(pt::Sampler& sampler, const pt::Aabb& bounds) {
    using pt::operator""_f;
    const pt::Float radius = 0.5_f * pt::Vec3(bounds.x.size(), bounds.y.size(), bounds.z.size()).length();
    const pt::Point3 origin = bounds.centroid() + 2.0_f * radius * pt::random_unit_vector(sampler);
    const pt::Point3 target = point_in(sampler, bounds);
    return pt::Ray(origin, target - origin);
}

[[nodiscard]] inline pt::Ray ray_inside(pt::Sampler& sampler, const pt::Aabb& bounds) {
    return pt::Ray(point_in(sampler, bounds), pt::random_unit_vector(sampler));
}

// A cloud of random triangles, without normals or uvs: the store then reports the flat normal and the raw
// barycentrics, which is exactly what a standalone Triangle reports, so the two can be compared bit for bit.
[[nodiscard]] inline pt::MeshData random_triangles(pt::Sampler& sampler, int count) {
    using pt::operator""_f;
    pt::MeshData data;
    for (int i = 0; i < count; ++i) {
        const pt::Point3 centre{sampler.next_scalar(-8.0_f, 8.0_f), sampler.next_scalar(-8.0_f, 8.0_f), sampler.next_scalar(-8.0_f, 8.0_f)};
        const auto first = static_cast<std::uint32_t>(data.positions.size());
        for (int k = 0; k < 3; ++k)
            data.positions.push_back(centre + pt::Vec3::random(-1.5_f, 1.5_f, sampler));
        data.indices.insert(data.indices.end(), {first, first + 1, first + 2});
    }
    return data;
}

} // namespace pt_test
