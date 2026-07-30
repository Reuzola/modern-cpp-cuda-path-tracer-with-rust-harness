#pragma once
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/math/vec3.hpp"
#include "pt/util/arena.hpp"

namespace pt {

class Material;

[[nodiscard]] const HittableList* box(Arena<Hittable>& arena, const Point3& a, const Point3& b, const Material* mat);

} // namespace pt
