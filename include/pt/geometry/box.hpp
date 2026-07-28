#pragma once
#include "pt/core/hittable_list.hpp"
#include "pt/math/vec3.hpp"
#include <memory>

namespace pt {

class material;

[[nodiscard]] std::shared_ptr<hittable_list> box(const point3& a, const point3& b, const material* mat);

} // namespace pt
