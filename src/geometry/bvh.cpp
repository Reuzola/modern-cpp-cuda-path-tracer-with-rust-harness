#include "pt/geometry/bvh.hpp"
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/util/arena.hpp"
#include <algorithm>
#include <chrono>
#include <span>
#include <vector>

namespace pt {

BvhNode::BvhNode(Arena<Hittable>& arena, const HittableList& list) {
    std::vector<const Hittable*> objects(list.objects().begin(), list.objects().end());
    build(arena, objects);
}

BvhNode::BvhNode(Arena<Hittable>& arena, std::span<const Hittable*> objects) { build(arena, objects); }

bool BvhNode::hit(const Ray& r, const Interval& ray_t, HitRecord& rec, Sampler& sampler) const {
    if (!bbox_.hit(r, ray_t)) return false;

    bool hit_left = left_->hit(r, ray_t, rec, sampler);
    bool hit_right = right_->hit(r, Interval(ray_t.min, hit_left ? rec.t : ray_t.max), rec, sampler);

    return hit_left || hit_right;
}

Aabb BvhNode::bounding_box() const { return bbox_; }

void BvhNode::build(Arena<Hittable>& arena, std::span<const Hittable*> objects) {
    bbox_ = Aabb();
    for (const Hittable* object : objects) {
        bbox_ = Aabb(bbox_, object->bounding_box());
    }

    int axis = bbox_.longest_axis();

    auto comparator = (axis == 0) ? box_x_compare : (axis == 1) ? box_y_compare
                                                                : box_z_compare;
    const auto count = objects.size();

    if (count == 1) {
        left_ = right_ = objects[0];
    } else if (count == 2) {
        left_ = objects[0];
        right_ = objects[1];
    } else {
        std::sort(objects.begin(), objects.end(), comparator);
        const auto mid = count / 2;

        left_ = arena.create<BvhNode>(arena, objects.subspan(0, mid));
        right_ = arena.create<BvhNode>(arena, objects.subspan(mid));

        left_is_node_ = true;
        right_is_node_ = true;
    }
}

void BvhNode::accumulate_stats(BvhStats& stats, int depth) const {
    ++stats.node_count;
    stats.max_depth = std::max(stats.max_depth, depth);

    // Safe static_cast: flags set by build() guarantee the child is a BvhNode, avoiding dynamic_cast overhead
    if (left_is_node_)
        static_cast<const BvhNode*>(left_)->accumulate_stats(stats, depth + 1);
    else
        ++stats.leaf_count;

    if (right_is_node_)
        static_cast<const BvhNode*>(right_)->accumulate_stats(stats, depth + 1);
    else
        ++stats.leaf_count;
}

bool BvhNode::box_compare(const Hittable* a, const Hittable* b, int axis_index) {
    const auto a_axis_interval = a->bounding_box().axis_interval(axis_index);
    const auto b_axis_interval = b->bounding_box().axis_interval(axis_index);
    return a_axis_interval.min < b_axis_interval.min;
}

bool BvhNode::box_x_compare(const Hittable* a, const Hittable* b) { return box_compare(a, b, 0); }
bool BvhNode::box_y_compare(const Hittable* a, const Hittable* b) { return box_compare(a, b, 1); }
bool BvhNode::box_z_compare(const Hittable* a, const Hittable* b) { return box_compare(a, b, 2); }

const BvhNode* make_bvh(Arena<Hittable>& arena, const HittableList& list, BvhStats* stats) {
    const auto start = std::chrono::steady_clock::now();
    const BvhNode* root = arena.create<BvhNode>(arena, list);
    const auto end = std::chrono::steady_clock::now();

    if (stats != nullptr) {
        stats->build_time += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        ++stats->bvh_count;
        root->accumulate_stats(*stats, 0);
    }

    return root;
}

} // namespace pt
