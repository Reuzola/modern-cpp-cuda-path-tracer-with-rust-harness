#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/materials/lambertian.hpp"
#include "pt/math/aabb.hpp"
#include "pt/math/color.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/scene/scene.hpp"
#include "pt/textures/solid_color.hpp"
#include "support/test_support.hpp"
#include <catch2/catch_test_macros.hpp>
#include <utility>

namespace {

using pt::Aabb;
using pt::Color;
using pt::Float;
using pt::HitRecord;
using pt::Hittable;
using pt::Interval;
using pt::Lambertian;
using pt::Point3;
using pt::Quad;
using pt::Ray;
using pt::Sampler;
using pt::Scene;
using pt::SolidColor;
using pt::Sphere;
using pt::Vec3;
using pt::operator""_f;
using pt_test::make_sampler;
using pt_test::require_near;

const Interval visible{0.001_f, pt::infinity};
const Ray forward{Point3(0, 0, 0), Vec3(0, 0, 1)};

int live_objects = 0;

/// A hittable that counts its own lifetime, to watch the arena's ownership.
class CountedHittable final : public Hittable {
public:
    CountedHittable() noexcept { ++live_objects; }

    CountedHittable(const CountedHittable&) = delete;
    CountedHittable& operator=(const CountedHittable&) = delete;

    ~CountedHittable() override { --live_objects; }

    [[nodiscard]] bool hit(const Ray&, const Interval&, HitRecord&) const override { return false; }

    [[nodiscard]] Aabb bounding_box() const override { return Aabb(Point3(-1, -1, -1), Point3(1, 1, 1)); }
};

} // namespace

TEST_CASE("a fresh scene is empty in every dimension", "[scene]") {
    const Scene scene;

    HitRecord rec;
    REQUIRE_FALSE(scene.world().hit(forward, visible, rec));
    REQUIRE(scene.importance_targets().empty());
    REQUIRE(scene.media().empty());
    REQUIRE(scene.mesh_count() == 0);

    // Defaults come from the settings structs, so a scene file may leave any of
    // them out and still describe a renderable image.
    REQUIRE(scene.render.image_width > 0);
    REQUIRE(scene.render.samples_per_pixel > 0);
}

TEST_CASE("objects added to a scene are in its world", "[scene]") {
    Scene scene;

    const SolidColor* grey = scene.create_texture<SolidColor>(Color(0.5_f, 0.5_f, 0.5_f));
    const Lambertian* material = scene.create_material<Lambertian>(grey);
    const Sphere* sphere = scene.create_object<Sphere>(Point3(0, 0, 5), 1.0_f, material);

    scene.add_object(sphere);

    HitRecord rec;
    REQUIRE(scene.world().hit(forward, visible, rec));
    require_near(rec.t, 4.0_f);

    // The record carries the material through, so a texture created here is
    // reachable from a hit without any further bookkeeping.
    REQUIRE(rec.mat == material);
}

TEST_CASE("the arenas own what the scene creates", "[scene]") {
    live_objects = 0;

    {
        Scene scene;
        for (int i = 0; i < 4; ++i) {
            scene.add_object(scene.create_object<CountedHittable>());
        }
        REQUIRE(live_objects == 4);
    }

    // Destroyed exactly once each, and in an order that works: the world holds
    // non-owning pointers into the object arena, and members are destroyed in
    // reverse declaration order, so the arena outlives the list that points into
    // it. Reordering those members compiles cleanly and leaves the destructor
    // reading freed memory.
    REQUIRE(live_objects == 0);
}

TEST_CASE("created objects keep their addresses as the scene grows", "[scene]") {
    Scene scene;

    const SolidColor* first = scene.create_texture<SolidColor>(Color(1, 0, 0));

    for (int i = 0; i < 500; ++i) {
        static_cast<void>(scene.create_texture<SolidColor>(Color(0, 1, 0)));
    }

    // The arena reallocates its pointer array many times over; the objects behind
    // those pointers never move. Materials hold raw texture pointers taken at
    // load time, so a loader that creates a texture early and a hundred more
    // afterwards depends on this.
    const SolidColor* last = scene.create_texture<SolidColor>(Color(0, 0, 1));
    REQUIRE(first != last);
    require_near(first->value(0, 0, Point3(0, 0, 0)).r(), 1.0_f);
}

TEST_CASE("meshes are owned outside the arena and counted", "[scene]") {
    const Scene scene;
    REQUIRE(scene.mesh_count() == 0);
}

TEST_CASE("importance targets are collected separately from the world", "[scene]") {
    Scene scene;

    const SolidColor* white = scene.create_texture<SolidColor>(Color(1, 1, 1));
    const Lambertian* material = scene.create_material<Lambertian>(white);
    const Quad* light = scene.create_object<Quad>(Point3(-1, -1, 2), Vec3(2, 0, 0), Vec3(0, 2, 0), material);

    scene.add_object(light);
    scene.add_importance_target(light);

    REQUIRE_FALSE(scene.importance_targets().empty());

    // The same object appears in both: one list is what a ray can hit, the other
    // is what a shading point should aim at. A light that was only in the second
    // would be sampled towards and then not be there.
    Sampler sampler = make_sampler(101);
    const Vec3 direction = scene.importance_targets().sample_direction(Point3(0, 0, 0), sampler);
    REQUIRE(scene.importance_targets().pdf_direction(Point3(0, 0, 0), direction) > 0.0_f);
}

TEST_CASE("media live beside the world, not inside it", "[scene]") {
    Scene scene;

    const SolidColor* smoke = scene.create_texture<SolidColor>(Color(0.2_f, 0.2_f, 0.2_f));
    const Lambertian* phase = scene.create_material<Lambertian>(smoke);
    const Sphere* boundary = scene.create_object<Sphere>(Point3(0, 0, 15), 10.0_f, phase);

    scene.add_medium(boundary, 0.5_f, phase);

    REQUIRE(scene.media().size() == 1);

    // The boundary is not added to the world: a volume is not a surface, and the
    // integrator walks the media list separately after its surface query. Adding
    // it to both would put a visible shell around every cloud.
    HitRecord rec;
    REQUIRE_FALSE(scene.world().hit(forward, visible, rec));

    Sampler sampler = make_sampler(102);
    REQUIRE(scene.media().front().sample_interaction(forward, visible, sampler, rec));
}

TEST_CASE("building the BVH replaces the world without changing it", "[scene]") {
    Scene scene;

    const SolidColor* grey = scene.create_texture<SolidColor>(Color(0.5_f, 0.5_f, 0.5_f));
    const Lambertian* material = scene.create_material<Lambertian>(grey);

    for (int i = 0; i < 16; ++i) {
        const Float z = 5.0_f + static_cast<Float>(i);
        scene.add_object(scene.create_object<Sphere>(Point3(0, 0, z), 0.25_f, material));
    }

    HitRecord before;
    REQUIRE(scene.world().hit(forward, visible, before));

    scene.build_bvh();

    HitRecord after;
    REQUIRE(scene.world().hit(forward, visible, after));

    // Same answer, different structure: the acceleration structure is an
    // optimisation and nothing else. The world is now a list of one - the tree's
    // root - and the primitives are still owned by the same arena.
    require_near(after.t, before.t);
    REQUIRE(after.mat == before.mat);

    // The build reports what it produced, which is what the baseline document is
    // written from.
    REQUIRE(scene.bvh_stats().bvh_count == 1);
    REQUIRE(scene.bvh_stats().node_count > 1);
    REQUIRE(scene.bvh_stats().leaf_count > 0);
    REQUIRE(scene.bvh_stats().max_depth > 0);
}

TEST_CASE("a moved scene still points at its own objects", "[scene]") {
    Scene source;

    const SolidColor* grey = source.create_texture<SolidColor>(Color(0.5_f, 0.5_f, 0.5_f));
    const Lambertian* material = source.create_material<Lambertian>(grey);
    source.add_object(source.create_object<Sphere>(Point3(0, 0, 5), 1.0_f, material));
    source.build_bvh();
    source.render.image_width = 321;

    const Scene moved = std::move(source);

    // The loader returns a Scene by value, so this move happens on every single
    // load. The arenas' vectors move; the objects behind their unique_ptrs do
    // not, which is the only reason the world's raw pointers survive the trip.
    HitRecord rec;
    REQUIRE(moved.world().hit(forward, visible, rec));
    require_near(rec.t, 4.0_f);
    REQUIRE(rec.mat == material);

    // Settings travel with it.
    REQUIRE(moved.render.image_width == 321);
    REQUIRE(moved.bvh_stats().bvh_count == 1);
}
