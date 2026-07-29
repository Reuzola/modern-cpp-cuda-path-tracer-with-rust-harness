#pragma once
#include "pt/core/hittable_list.hpp"
#include "pt/math/vec3.hpp"
#include <memory>

namespace pt {

class Material;

[[nodiscard]] std::shared_ptr<HittableList> box(const Point3& a, const Point3& b, const Material* mat);

} // namespace pt
