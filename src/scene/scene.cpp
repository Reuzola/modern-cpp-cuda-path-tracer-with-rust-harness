#include "pt/scene/scene.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/core/sampleable.hpp"
#include "pt/geometry/bvh.hpp"

namespace pt {

void Scene::add_object(const Hittable* obj) { world_.add(obj); }

void Scene::add_importance_target(const Sampleable* target) { importance_targets_.add(target); }

void Scene::build_bvh() {
    const Hittable* root = objects_.create<BvhNode>(objects_, world_);
    world_ = HittableList(root);
}

} // namespace pt
