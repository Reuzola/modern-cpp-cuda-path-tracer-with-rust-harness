#include "pt/scene/scene.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/core/sampleable.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/scalar.hpp"

namespace pt {

void Scene::add_object(const Hittable* obj) { world_.add(obj); }

void Scene::add_importance_target(const Sampleable* target) { importance_targets_.add(target); }

void Scene::build_bvh() {
    const Hittable* root = make_bvh(objects_, world_, &bvh_stats_);
    world_ = HittableList(root);
}

void Scene::add_medium(const Hittable* boundary, Float density, const Material* phase_function) {
    media_.emplace_back(boundary, density, phase_function);
}

} // namespace pt
