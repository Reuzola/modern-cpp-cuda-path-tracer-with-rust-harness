#include "pt/scene/scene.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/geometry/bvh.hpp"
#include <memory>
#include <utility>

namespace pt {

void Scene::add_object(std::shared_ptr<Hittable> obj) { world_.add(std::move(obj)); }

void Scene::add_light(std::shared_ptr<Hittable> obj) { lights_.add(std::move(obj)); }

void Scene::build_bvh() { world_ = HittableList(std::make_shared<BvhNode>(world_)); }

} // namespace pt
