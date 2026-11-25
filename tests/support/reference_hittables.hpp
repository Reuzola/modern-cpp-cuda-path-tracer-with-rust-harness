#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/vec3.hpp"
#include <span>
#include <vector>

namespace pt {
class Material;
} // namespace pt

// Brute-force references the trees are tested against. The engine uses neither: a test asks a tree and one
// of these the same question and requires the same answer, bit for bit.
namespace pt_test {

// Every object asked in turn, the interval narrowed to the nearest hit so far. One record is reused across
// candidates, so a hittable that leaves a field unwritten leaks the previous candidate's value into it.
class HittableList final : public pt::Hittable {
public:
    HittableList() = default;
    explicit HittableList(const pt::Hittable* object);

    [[nodiscard]] bool hit(const pt::Ray& r, const pt::Interval& ray_t, pt::HitRecord& rec) const override;

    [[nodiscard]] pt::Aabb bounding_box() const override;

    void clear();

    void add(const pt::Hittable* object);

    [[nodiscard]] bool empty() const noexcept { return objects_.empty(); }

    [[nodiscard]] std::span<const pt::Hittable* const> objects() const noexcept { return objects_; }

private:
    std::vector<const pt::Hittable*> objects_;
    pt::Aabb bbox_;
};

// One triangle on its own, filled the way a mesh without normals or uvs is: the flat normal, and the raw
// barycentrics as uv. That match is what lets a tree over a mesh be compared with a scan over these.
class Triangle final : public pt::Hittable {
public:
    Triangle(const pt::Point3& v0, const pt::Point3& v1, const pt::Point3& v2, const pt::Material* mat);

    [[nodiscard]] bool hit(const pt::Ray& r, const pt::Interval& ray_t, pt::HitRecord& rec) const override;

    [[nodiscard]] pt::Aabb bounding_box() const override;

private:
    pt::Point3 v0_;
    pt::Point3 v1_;
    pt::Point3 v2_;
    const pt::Material* mat_ = nullptr;
    pt::Aabb bbox_;
};

} // namespace pt_test
