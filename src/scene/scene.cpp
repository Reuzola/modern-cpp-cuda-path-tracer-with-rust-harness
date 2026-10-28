#include "pt/scene/scene.hpp"
#include "pt/core/sampleable.hpp"
#include "pt/geometry/scene_graph.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/scalar.hpp"
#include <cassert>
#include <span>

namespace pt {

void Scene::add_object(SceneNode object) {
    assert(!built_);
    roots_.push_back(object);
}

void Scene::add_importance_target(const Sampleable* target) { importance_targets_.add(target); }

LevelPlan Scene::plan() const { return plan_levels(roots_); }

void Scene::build_bvh() {
    assert(!built_);
    world_ = build_levels(plan(), &bvh_stats_);
    built_ = true;
}

void Scene::add_medium(SceneNode boundary, Float density, const Material* phase_function) {
    const std::span<const SceneNode> roots(&boundary, 1);
    media_.emplace_back(build_levels(plan_levels(roots), &bvh_stats_), density, phase_function);
}

} // namespace pt
