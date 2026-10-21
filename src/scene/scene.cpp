#include "pt/scene/scene.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/core/sampleable.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/group_builder.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/scalar.hpp"
#include <cassert>
#include <utility>

namespace pt {

const Hittable& Scene::world() const noexcept {
    // Immutable, so one empty world serves every scene that is not built yet.
    static const HittableList nothing;
    return world_ != nullptr ? *world_ : nothing;
}

void Scene::add_object(GeometryRef object) {
    assert(world_ == nullptr);
    staged_.add(object);
}

void Scene::add_importance_target(const Sampleable* target) { importance_targets_.add(target); }

void Scene::build_bvh() {
    assert(world_ == nullptr);
    world_ = std::move(staged_).build(objects_, &bvh_stats_);
}

void Scene::add_medium(GeometryRef boundary, Float density, const Material* phase_function) {
    // The boundary is hit through a set like any surface, but it never joins the world.
    media_.emplace_back(seal(objects_, boundary, &bvh_stats_), density, phase_function);
}

} // namespace pt
